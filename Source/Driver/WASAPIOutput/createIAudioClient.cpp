// Copyright (C) 2023 Hyun Woo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.


#include "createIAudioClient.h"
#include "../utils/raiiUtils.h"
#include "../utils/WASAPIUtils.h"
#include <spdlog/spdlog.h>
#include "../utils/logger.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>

static void dumpErrorWaveFormatEx(const char *varname, const WAVEFORMATEX &pWaveFormat) {
    mainlog->error("    : {}.wFormatTag: {}", varname, pWaveFormat.wFormatTag);
    mainlog->error("    : {}.nChannels: {}", varname, pWaveFormat.nChannels);
    mainlog->error("    : {}.nSamplesPerSec: {}", varname, pWaveFormat.nSamplesPerSec);
    mainlog->error("    : {}.nAvgBytesPerSec: {}", varname, pWaveFormat.nAvgBytesPerSec);
    mainlog->error("    : {}.nBlockAlign: {}", varname, pWaveFormat.nBlockAlign);
    mainlog->error("    : {}.cbSize: {}", varname, pWaveFormat.cbSize);
}

std::shared_ptr<IAudioClient>
createAudioClient(const std::shared_ptr<IMMDevice> &pDevice, WAVEFORMATEX *pWaveFormat, int bufferSizeRequest,
                  WASAPIMode mode) {
    SPDLOG_TRACE_FUNC;

    if (!pDevice || !pWaveFormat) {
        return nullptr;
    }

    // WASAPI flags
    auto shareMode = (mode == WASAPIMode::Event) ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
    auto streamFlags =
            (mode == WASAPIMode::Event) ? AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST
                                        : AUDCLNT_STREAMFLAGS_NOPERSIST;

    ////

    HRESULT hr;
    auto deviceId = getDeviceId(pDevice);

    IAudioClient *pAudioClient_ = nullptr;
    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **) &pAudioClient_);
    if (FAILED(hr) || !pAudioClient_) {
        mainlog->error(L"{} pAudioClient->Activate failed: 0x{:08X}", deviceId, (uint32_t) hr);
        return nullptr;
    }
    auto pAudioClient = make_autorelease(pAudioClient_);

    if (mode == WASAPIMode::Event) {
        hr = pAudioClient->IsFormatSupported(shareMode, pWaveFormat, nullptr);
        if (FAILED(hr)) {
            mainlog->error(L"{} pAudioClient->IsFormatSupported failed: 0x{:08X}", deviceId, (uint32_t) hr);
            return nullptr;
        }
    } else {
        WAVEFORMATEX *pClosestMatch;
        hr = pAudioClient->IsFormatSupported(shareMode, pWaveFormat, &pClosestMatch);
        if (hr == S_FALSE) {
            mainlog->error(L"{} pAudioClient->IsFormatSupported failed: S_FALSE (see pClosestMatch)", deviceId);
            dumpErrorWaveFormatEx("pWaveFormat", *pWaveFormat);
            dumpErrorWaveFormatEx("pClosestMatch", *pClosestMatch);
            CoTaskMemFree(pClosestMatch);
            return nullptr;
        }
        if (FAILED(hr)) {
            mainlog->error(L"{} pAudioClient->IsFormatSupported failed: 0x{:08X}", deviceId, (uint32_t) hr);
            return nullptr;
        }
    }


    REFERENCE_TIME defaultBufferDuration, minBufferDuration, bufferDuration;
    hr = pAudioClient->GetDevicePeriod(&defaultBufferDuration, &minBufferDuration);
    if (FAILED(hr)) return nullptr;
    mainlog->info(L"{} minimum duration {}, default duration {}", deviceId, minBufferDuration,
                  defaultBufferDuration);

    if (bufferSizeRequest == BUFFER_SIZE_REQUEST_USEDEFAULT) {
        bufferDuration = defaultBufferDuration;
    } else {
        bufferDuration = (REFERENCE_TIME) lround(10000.0 *                         // (REFERENCE_TIME / ms) *
                                                                                   1000 *                            // (ms / s) *
                                                                                   bufferSizeRequest /                      // frames /
                                                                                   pWaveFormat->nSamplesPerSec      // (frames / s)
        );
    }
    if (bufferDuration < minBufferDuration) {
        bufferDuration = minBufferDuration;
    }


    mainlog->debug(L"{} pAudioClient->Initialize: bufferSizeRequest {}, bufferDuration {.1f}ms", deviceId,
                   bufferSizeRequest,
                   bufferDuration / 1000.0);

    hr = pAudioClient->Initialize(
            shareMode,
            streamFlags,
            bufferDuration,
            bufferDuration,
            pWaveFormat,
            nullptr);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        // https://learn.microsoft.com/en-US/windows/win32/api/audioclient/nf-audioclient-iaudioclient-initialize
        mainlog->debug(L"{} pAudioClient->Initialize: AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED -> re-initializing",
                       deviceId);

        UINT32 nextHighestAlignedBufferSize = 0;
        hr = pAudioClient->GetBufferSize(&nextHighestAlignedBufferSize);
        if (FAILED(hr)) return nullptr;
        auto alignedBufferDuration = (REFERENCE_TIME) lround(
                10000.0 * 1000 / pWaveFormat->nSamplesPerSec * nextHighestAlignedBufferSize);
        mainlog->debug(L"{} nextHighestAlignedBufferSize {}, alignedBufferDuration {}", nextHighestAlignedBufferSize,
                       alignedBufferDuration);

        pAudioClient = nullptr;
        hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **) &pAudioClient_);
        if (FAILED(hr) || !pAudioClient_) {
            mainlog->error(L"{} pAudioClient->Activate failed: 0x{:08X}", deviceId, (uint32_t) hr);
            return nullptr;
        }
        pAudioClient = make_autorelease(pAudioClient_);

        hr = pAudioClient->Initialize(
                shareMode,
                streamFlags,
                alignedBufferDuration,
                alignedBufferDuration,
                pWaveFormat,
                nullptr);
    }

    if (FAILED(hr)) {
        mainlog->error(L"{} pAudioClient->Initialize failed: 0x{:08X}", deviceId, (uint32_t) hr);
        dumpErrorWaveFormatEx("pWaveFormat", *pWaveFormat);
        return nullptr;
    }

    return pAudioClient;
}

bool FindStreamFormat(
        const std::shared_ptr<IMMDevice> &pDevice,
        int nChannels,
        int nSampleRate,
        int bufferSizeRequest,
        WASAPIMode mode,
        WAVEFORMATEXTENSIBLE *pwfxt,
        std::shared_ptr<IAudioClient> *ppAudioClient) {

    SPDLOG_TRACE_FUNC;

    if (!pDevice) return false;

    // create a reasonable channel mask
    DWORD dwChannelMask = (1 << nChannels) - 1;
    WAVEFORMATEXTENSIBLE waveFormat;

    // try 16-bit first
    waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    waveFormat.Format.nChannels = nChannels;
    waveFormat.Format.nSamplesPerSec = nSampleRate;
    waveFormat.Format.wBitsPerSample = 16;
    waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample / 8 * waveFormat.Format.nChannels;
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    waveFormat.Samples.wValidBitsPerSample = waveFormat.Format.wBitsPerSample;
    waveFormat.dwChannelMask = dwChannelMask;
    waveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    auto pAudioClient = createAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat, bufferSizeRequest, mode);
    if (pAudioClient) {
        goto Finish;
    }

    Finish:
    bool bSuccess = (pAudioClient != nullptr);
    if (bSuccess) {
        if (pwfxt)
            memcpy_s(pwfxt, sizeof(WAVEFORMATEXTENSIBLE), &waveFormat, sizeof(WAVEFORMATEXTENSIBLE));
        if (ppAudioClient)
            *ppAudioClient = pAudioClient;
    }
    return bSuccess;
}

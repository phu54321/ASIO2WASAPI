//
// Created by whyask37 on 2023-06-27.
//

#include "createIAudioClient.h"
#include "../utils/raiiUtils.h"
#include "../utils/WASAPIUtils.h"
#include <spdlog/spdlog.h>
#include "../utils/logger.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>

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
    if (FAILED(hr) || !pAudioClient_) return nullptr;
    auto pAudioClient = make_autorelease(pAudioClient_);

    hr = pAudioClient->IsFormatSupported(shareMode, pWaveFormat, nullptr);
    if (FAILED(hr)) return nullptr;


    REFERENCE_TIME defaultBufferDuration, minBufferDuration, bufferDuration;
    hr = pAudioClient->GetDevicePeriod(&defaultBufferDuration, &minBufferDuration);
    if (FAILED(hr)) return nullptr;
    mainlog->debug(L"{} - minimum duration {}, default duration {}", deviceId, minBufferDuration,
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


    mainlog->debug(L"pAudioClient->Initialize: device {}, bufferSizeRequest {}, bufferDuration {}", deviceId,
                   bufferSizeRequest,
                   bufferDuration);
    hr = pAudioClient->Initialize(
            shareMode,
            streamFlags,
            bufferDuration,
            bufferDuration,
            pWaveFormat,
            nullptr);

    if (FAILED(hr)) {
        mainlog->error(" - pAudioClient->Initialize failed (0:08x})", hr);
        mainlog->error("    : pWaveFormat->wFormatTag: {}", pWaveFormat->wFormatTag);
        mainlog->error("    : pWaveFormat->nChannels: {}", pWaveFormat->nChannels);
        mainlog->error("    : pWaveFormat->nSamplesPerSec: {}", pWaveFormat->nSamplesPerSec);
        mainlog->error("    : pWaveFormat->nAvgBytesPerSec: {}", pWaveFormat->nAvgBytesPerSec);
        mainlog->error("    : pWaveFormat->nBlockAlign: {}", pWaveFormat->nBlockAlign);
        mainlog->error("    : pWaveFormat->cbSize: {}", pWaveFormat->cbSize);
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

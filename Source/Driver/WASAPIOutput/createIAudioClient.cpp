//
// Created by whyask37 on 2023-06-27.
//

#include "createIAudioClient.h"
#include "../logger.h"
#include "../raiiUtils.h"

#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

const IID IID_IAudioClient = __uuidof(IAudioClient);

std::shared_ptr<IAudioClient> createAudioClient(const std::shared_ptr<IMMDevice> &pDevice, WAVEFORMATEX *pWaveFormat) {
    if (!pDevice || !pWaveFormat) {
        return nullptr;
    }

    HRESULT hr;

    IAudioClient *pAudioClient_ = nullptr;
    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **) &pAudioClient_);
    if (FAILED(hr) || !pAudioClient_) return nullptr;
    auto pAudioClient = make_autorelease(pAudioClient_);

    hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, pWaveFormat, nullptr);
    if (FAILED(hr)) return nullptr;


    // calculate buffer size and duration
    REFERENCE_TIME hnsDefaultDuration = 0;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDuration, nullptr);
    if (FAILED(hr)) return nullptr;

    hnsDefaultDuration = max(hnsDefaultDuration, (REFERENCE_TIME) 1000000); // 100ms minimum

    hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsDefaultDuration,
            hnsDefaultDuration,
            pWaveFormat,
            nullptr);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        UINT bufferSize = 0;
        hr = pAudioClient->GetBufferSize(&bufferSize);
        if (FAILED(hr)) {
            Logger::error(L"pAudioClient->GetBufferSize failed");
            return nullptr;
        }
        pAudioClient = nullptr;

        const double REFTIME_UNITS_PER_SECOND = 10000000.;
        auto hnsAlignedDuration = static_cast<REFERENCE_TIME>(round(
                bufferSize / (pWaveFormat->nSamplesPerSec / REFTIME_UNITS_PER_SECOND)));

        hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **) &pAudioClient_);
        if (FAILED(hr) || !pAudioClient_)
            return nullptr;
        pAudioClient.reset(pAudioClient_);

        hr = pAudioClient->Initialize(
                AUDCLNT_SHAREMODE_EXCLUSIVE,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                hnsAlignedDuration,
                hnsAlignedDuration,
                pWaveFormat,
                nullptr);
    }
    if (FAILED(hr)) return nullptr;
    return pAudioClient;
}

bool FindStreamFormat(
        const std::shared_ptr<IMMDevice> &pDevice,
        int nChannels,
        int nSampleRate,
        WAVEFORMATEXTENSIBLE *pwfxt,
        std::shared_ptr<IAudioClient> *ppAudioClient) {

    LOGGER_TRACE_FUNC;

    if (!pDevice) return false;

    // create a reasonable channel mask
    DWORD dwChannelMask = (1 << nChannels) - 1;
    // DWORD dwChannelMask = 0;
    // DWORD bit = 1;
    // for (int i = 0; i < nChannels; i++)
    // {
    //     dwChannelMask |= bit;
    //     bit <<= 1;
    // }

    WAVEFORMATEXTENSIBLE waveFormat;

    // try 16-bit first
    Logger::debug(L"Trying 16-bit packed");
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

    auto pAudioClient = createAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat);
    if (pAudioClient) {
        Logger::debug(L" - works!");
        goto Finish;
    }

    Logger::debug(L" - none works");

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

//
// Created by whyask37 on 2023-07-01.
//
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <cassert>

#include "WASAPIOutput.h"
#include "WASAPIOutputPush.h"
#include "createIAudioClient.h"
#include "../utils/WASAPIUtils.h"
#include "../utils/raiiUtils.h"
#include "../utils/logger.h"
#include "../utils/AppException.h"

const double m_pi = 3.14159265358979;

const int inBufferSizeMultiplier = 8;

WASAPIOutputPush::WASAPIOutputPush(
        const std::shared_ptr<IMMDevice> &pDevice,
        int channelNum,
        int sampleRate,
        int bufferSizeRequest)
        : _pDevice(pDevice), _channelNum(channelNum), _sampleRate(sampleRate) {

    SPDLOG_TRACE_FUNC;

    _pDeviceId = getDeviceId(pDevice);

    if (!FindStreamFormat(pDevice, channelNum, sampleRate, bufferSizeRequest, WASAPIMode::Pull, &_waveFormat,
                          &_pAudioClient)) {
        mainlog->error(L"Cannot find suitable stream format for output _pDevice (_pDevice ID {})", _pDeviceId);
        throw AppException("FindStreamFormat failed");
    }

    UINT32 bufferSize;
    HRESULT hr = _pAudioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr)) {
        throw AppException("GetBufferSize failed");
    }
    if (bufferSize < bufferSizeRequest) {
        mainlog->error(" - Buffer size too small: requested {}, got {}", bufferSizeRequest, bufferSize);
        throw AppException("Too low buffer size");
    }
    mainlog->info(L"WASAPIOutputPush: {} - Buffer size {}", _pDeviceId, bufferSize);
    _outBufferSize = bufferSizeRequest;

    IAudioRenderClient *pRenderClient_ = nullptr;
    hr = _pAudioClient->GetService(
            IID_IAudioRenderClient,
            (void **) &pRenderClient_);
    if (FAILED(hr)) {
        throw AppException("IAudioRenderClient init failed");
    }
    _pRenderClient = make_autorelease(pRenderClient_);

    _pAudioClient->Start();
}

WASAPIOutputPush::~WASAPIOutputPush() {
    SPDLOG_TRACE_FUNC;
    _pAudioClient->Stop();
}


void WASAPIOutputPush::pushSamples(const std::vector<std::vector<short>> &buffer) {
    SPDLOG_TRACE_FUNC;

    assert (buffer.size() == _channelNum);

    if (buffer.size() != _channelNum) {
        mainlog->error(L"{} Invalid channel count: expected {}, got {}", _pDeviceId, _channelNum, buffer.size());
        return;
    }

    if (buffer[0].size() != _outBufferSize) {
        mainlog->error(L"{} Invalid chunk length: expected {}, got {}", _pDeviceId, _outBufferSize, buffer[0].size());
        return;
    }

    mainlog->debug(L"{} pushSamples, buffersize {}", _pDeviceId, _outBufferSize);


    BYTE *pData;
    HRESULT hr = _pRenderClient->GetBuffer(_outBufferSize, &pData);
    if (FAILED(hr)) {
        mainlog->error(L"{} _pRenderClient->GetBuffer() failed, (0x{0:08X})", _pDeviceId, hr);
        return;
    }

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;
    assert(sampleSize == 2);

    auto out = reinterpret_cast<short *>(pData);
    for (int i = 0; i < _outBufferSize; i++) {
        for (unsigned channel = 0; channel < _channelNum; channel++) {
            *(out++) = buffer[channel][i];
        }
    }

    _pRenderClient->ReleaseBuffer(_outBufferSize, 0);
}

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

// WASAPI shared mode keeps its own queue, so it needs some padding to prevent write overflow
const int outBufferSizeMultiplier = 4;
const int minimumPushBuferSize = 1024;

WASAPIOutputPush::WASAPIOutputPush(
        const std::shared_ptr<IMMDevice> &pDevice,
        int channelNum,
        int sampleRate,
        int bufferSizeRequest)
        : _pDevice(pDevice), _channelNum(channelNum), _sampleRate(sampleRate) {

    SPDLOG_TRACE_FUNC;

    _pDeviceId = getDeviceId(pDevice);

    _outBufferSize = bufferSizeRequest;

    // WASAPI shared mode keeps its own queue, so it needs some spacing.
    bufferSizeRequest = max(bufferSizeRequest * outBufferSizeMultiplier, minimumPushBuferSize);

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


void WASAPIOutputPush::pushSamples(const std::vector<std::vector<int32_t>> &buffer) {
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
        if (hr == AUDCLNT_E_BUFFER_TOO_LARGE) {
            mainlog->warn(L"{} [++++++++++] Write overflow!", _pDeviceId);
        } else {
            mainlog->error(L"{} _pRenderClient->GetBuffer() failed, (0x{:08X})", _pDeviceId, (uint32_t) hr);
        }
        return;
    }

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;

    if (sampleSize == 2) {
        auto out = reinterpret_cast<int16_t *>(pData);
        for (int i = 0; i < _outBufferSize; i++) {
            for (unsigned channel = 0; channel < _channelNum; channel++) {
                *(out++) = (int16_t) (buffer[channel][i] >> 16);
            }
        }
    } else if (sampleSize == 4) {
        auto out = reinterpret_cast<int32_t *>(pData);
        for (int i = 0; i < _outBufferSize; i++) {
            for (unsigned channel = 0; channel < _channelNum; channel++) {
                *(out++) = buffer[channel][i];
            }
        }
    }

    _pRenderClient->ReleaseBuffer(_outBufferSize, 0);
}

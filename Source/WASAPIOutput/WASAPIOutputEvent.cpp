// Copyright (C) 2023 Hyun Woo Park
//
// This file is part of trgkASIO.
//
// trgkASIO is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// trgkASIO is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.


#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <cassert>
#include <mutex>
#include <cstdlib>

#include "WASAPIOutput.h"
#include "WASAPIOutputEvent.h"
#include "createIAudioClient.h"
#include <spdlog/spdlog.h>
#include "../utils/WASAPIUtils.h"
#include "../utils/raiiUtils.h"
#include "../utils/logger.h"
#include "../utils/AppException.h"
#include <tracy/Tracy.hpp>


WASAPIOutputEvent::WASAPIOutputEvent(
        const std::shared_ptr<IMMDevice> &pDevice,
        UserPrefPtr pref,
        int sampleRate,
        UINT32 inputBufferSize,
        WASAPIMode mode,
        int ringBufferSizeMultiplier)
        : _pDevice(pDevice), _inputBufferSize(inputBufferSize), _channelNum(pref->channelCount),
          _sampleRate(sampleRate), _mode(mode) {

    ZoneScoped;
    HRESULT hr;

    _pDeviceId = getDeviceId(pDevice);
    if (!FindStreamFormat(pDevice, pref, sampleRate, mode, &_waveFormat,
                          &_pAudioClient)) {
        mainlog->error(L"{} Cannot find suitable stream for mat for output _pDevice", _pDeviceId);
        throw AppException("FindStreamFormat failed");
    }

    hr = _pAudioClient->GetBufferSize(&_outputBufferSize);
    if (FAILED(hr)) {
        throw AppException("GetBufferSize failed");
    }
    mainlog->info(L"{} WASAPIOutputEvent: - Buffer size: input {}, output {}",
                  _pDeviceId, _inputBufferSize, _outputBufferSize);

    size_t ringBufferSize = (_inputBufferSize + _outputBufferSize) * ringBufferSizeMultiplier;
    for (int i = 0; i < _channelNum; i++) {
        _ringBufferList.emplace_back(ringBufferSize);
    }

    _loadDataBuffer.resize(_outputBufferSize);

    // TODO: start only when sufficient data is fetched.
    start();
}

WASAPIOutputEvent::~WASAPIOutputEvent() {
    ZoneScoped;

    stop();
    WaitForSingleObject(_runningEvent, INFINITE);
}

void WASAPIOutputEvent::start() {
    ZoneScoped;

    // Wait for previous wasapi thread to close
    WaitForSingleObject(_runningEvent, INFINITE);
    if (!_stopEvent) {
        _stopEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        CreateThread(nullptr, 0, playThread, this, 0, nullptr);
    }
}

void WASAPIOutputEvent::stop() {
    ZoneScoped;

    if (_stopEvent) {
        SetEvent(_stopEvent);
        CloseHandle(_stopEvent);
        _stopEvent = nullptr;
    }
}


void WASAPIOutputEvent::pushSamples(const std::vector<std::vector<int32_t>> &buffer) {
    ZoneScoped;

    assert (buffer.size() == _channelNum);

    auto inputSize = buffer[0].size();

    {
        auto &rb0 = _ringBufferList[0];
        mainlog->trace(L"{} pushSamples, rp {} wp {} _ringBufferSize {} _inputBufferSize {}, _outputBufferSize {}",
                       _pDeviceId, rb0.rp(), rb0.wp(),
                       rb0.capacity(), _inputBufferSize, _outputBufferSize);
    }

    {
        ZoneScopedN("Validating argument buffer size");
        if (buffer.size() != _channelNum) {
            mainlog->error(L"{} Invalid channel count: expected {}, got {}", _pDeviceId, _channelNum, buffer.size());
            return;
        }

        if (buffer[0].size() != _inputBufferSize) {
            mainlog->error(L"{} Invalid chunk length: expected {}, got {}", _pDeviceId, _inputBufferSize,
                           buffer[0].size());
            return;
        }
    }

    bool write_overflow = false;
    {
        ZoneScopedN("Buffer copying");

        std::lock_guard guard(_ringBufferMutex);
        if (_ringBufferList[0].size() + inputSize >= _ringBufferList[0].capacity()) {
            write_overflow = true;
        } else {
            for (int ch = 0; ch < _channelNum; ch++) {
                auto res = _ringBufferList[ch].push(buffer[ch].data(), inputSize);
                assert(res);
            }
        }
    }

    // Logging may take long, so do this outside of the mutex
    if (write_overflow) {
        mainlog->warn(L"{} [++++++++++] Write overflow!", _pDeviceId);
        return;
    }
}


HRESULT WASAPIOutputEvent::LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient) {
    if (!pRenderClient) {
        return E_INVALIDARG;
    }

    ZoneScoped;

    BYTE *pData;
    HRESULT hr;

    size_t writeBufferSize = _outputBufferSize;
    if (_mode == WASAPIMode::Shared) {
        UINT32 paddingFrames = 0;
        _pAudioClient->GetCurrentPadding(&paddingFrames);
        writeBufferSize -= paddingFrames;
    }

    {
        ZoneScopedN("GetBuffer");
        hr = pRenderClient->GetBuffer(writeBufferSize, &pData);
        if (FAILED(hr)) {
            mainlog->error(L"{} _pRenderClient->GetBuffer({}) failed, (0x{:08X})", _pDeviceId, writeBufferSize,
                           (uint32_t) hr);
            return E_FAIL;
        }
    }

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;

    {
        auto &rb0 = _ringBufferList[0];
        mainlog->debug(L"{} LoadData, rp {} wp {} ringSize {} get {}", _pDeviceId, rb0.rp(), rb0.wp(),
                       rb0.capacity(), writeBufferSize);
    }

    bool skipped = false;
    {
        ZoneScopedN("Buffer copying");

        std::lock_guard guard(_ringBufferMutex);

        if (_ringBufferList[0].size() < writeBufferSize) {
            memset(pData, 0, sampleSize * _channelNum * writeBufferSize);
            skipped = true;
        } else {
            for (unsigned ch = 0; ch < _channelNum; ch++) {
                _ringBufferList[ch].get(_loadDataBuffer.data(), writeBufferSize);
                if (sampleSize == 2) {
                    auto out = reinterpret_cast<int16_t *>(pData) + ch;
                    auto pStart = _loadDataBuffer.data();
                    auto pEnd = _loadDataBuffer.data() + writeBufferSize;
                    for (auto p = pStart; p != pEnd; p++) {
                        *out = (int16_t) ((*p) >> 16);
                        out += _channelNum;
                    }
                } else if (sampleSize == 4) {
                    auto out = reinterpret_cast<int32_t *>(pData) + ch;
                    auto pStart = _loadDataBuffer.data();
                    auto pEnd = _loadDataBuffer.data() + writeBufferSize;
                    for (auto p = pStart; p != pEnd; p++) {
                        *out = *p;
                        out += _channelNum;
                    }
                }
            }
        }
    }

    // mainlog->warn may take long, so this logging should be done outside _ringBufferMutex lock.
    if (skipped) {
        mainlog->warn(L"{} [----------] Skipped pushing to wasapi", _pDeviceId);
    }

    {
        ZoneScopedN("ReleaseBuffer");
        pRenderClient->ReleaseBuffer(writeBufferSize, 0);
    }
    return S_OK;
}

/////

DWORD WINAPI WASAPIOutputEvent::playThread(LPVOID pThis) {
    HRESULT hr;

    auto *pDriver = static_cast<WASAPIOutputEvent *>(pThis);
    pDriver->_runningEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    CExitEventSetter setter(pDriver->_runningEvent);

    auto pAudioClient = pDriver->_pAudioClient;

    hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return -1;

    // Create an event handle and register it for
    // buffer-event notifications.
    HANDLE hEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    auto hEvent = make_autoclose(hEvent_);

    hr = pAudioClient->SetEventHandle(hEvent.get());
    if (FAILED(hr))
        return -1;

    IAudioRenderClient *pRenderClient_ = nullptr;
    hr = pAudioClient->GetService(
            IID_IAudioRenderClient,
            (void **) &pRenderClient_);
    if (FAILED(hr))
        return -1;
    auto pRenderClient = make_autorelease(pRenderClient_);

    // Ask MMCSS to temporarily boost the runThread priority
    // to reduce the possibility of glitches while we play.
    DWORD taskIndex = 0;
    AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);

    // Pre-load the first buffer with data
    // from the audio source before starting the stream.
    hr = pDriver->LoadData(pRenderClient);
    if (FAILED(hr))
        return -1;

    hr = pAudioClient->Start(); // Start playing.
    if (FAILED(hr))
        return -1;

    HANDLE events[2] = {pDriver->_stopEvent, hEvent.get()};
    while ((WaitForMultipleObjects(2, events, FALSE, INFINITE)) ==
           (WAIT_OBJECT_0 + 1)) { // the hEvent is signalled and m_hStopPlayThreadEvent is not
        // Grab the next empty buffer from the audio _pDevice.
        ZoneScopedN("WASAPIOutputEvent::playThread");
//        mainlog->trace("WaitForMultipleObjects");
        pDriver->LoadData(pRenderClient);
    }

    hr = pAudioClient->Stop(); // Stop playing.
    if (FAILED(hr))
        return -1;

    return 0;
}

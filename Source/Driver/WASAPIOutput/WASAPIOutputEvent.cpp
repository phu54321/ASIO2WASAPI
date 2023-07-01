//
// Created by whyask37 on 2023-06-26.
//


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

const int inBufferSizeMultiplier = 8;


WASAPIOutputEvent::WASAPIOutputEvent(
        const std::shared_ptr<IMMDevice> &pDevice,
        int channelNum,
        int sampleRate,
        int bufferSizeRequest,
        std::function<void()> eventCallback)
        : _pDevice(pDevice), _channelNum(channelNum), _sampleRate(sampleRate), _eventCallback(eventCallback) {

    SPDLOG_TRACE_FUNC;

    _pDeviceId = getDeviceId(pDevice);

    if (!FindStreamFormat(pDevice, channelNum, sampleRate, bufferSizeRequest, WASAPIMode::Event, &_waveFormat,
                          &_pAudioClient)) {
        mainlog->error(L"Cannot find suitable stream format for output _pDevice (_pDevice ID {})", _pDeviceId);
        throw AppException("FindStreamFormat failed");
    }

    UINT32 bufferSize;
    HRESULT hr = _pAudioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr)) {
        throw AppException("GetBufferSize failed");
    }

    mainlog->info(L"WASAPIOutputEvent: {} - Buffer size {}", _pDeviceId, bufferSize);
    _outBufferSize = bufferSize;

    _ringBufferSize = _outBufferSize * inBufferSizeMultiplier;
    _ringBuffer.resize(channelNum);
    for (int i = 0; i < channelNum; i++) {
        _ringBuffer[i].resize(_ringBufferSize);
        std::fill(_ringBuffer[i].begin(), _ringBuffer[i].end(), 0);
    }

    // TODO: start only when sufficient data is fetched.
    start();
}

WASAPIOutputEvent::~WASAPIOutputEvent() {
    SPDLOG_TRACE_FUNC;

    stop();
    WaitForSingleObject(_runningEvent, INFINITE);
}

void WASAPIOutputEvent::start() {
    SPDLOG_TRACE_FUNC;

    // Wait for previous wasapi thread to close
    WaitForSingleObject(_runningEvent, INFINITE);
    if (!_stopEvent) {
        _stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        CreateThread(nullptr, 0, playThread, this, 0, nullptr);
    }
}

void WASAPIOutputEvent::stop() {
    SPDLOG_TRACE_FUNC;

    if (_stopEvent) {
        SetEvent(_stopEvent);
        CloseHandle(_stopEvent);
        _stopEvent = nullptr;
    }
}


void WASAPIOutputEvent::pushSamples(const std::vector<std::vector<short>> &buffer) {
    assert (buffer.size() == _channelNum);

    mainlog->trace(L"{} pushSamples, rp {} wp {} bufferSize {}", _pDeviceId, _ringBufferReadPos, _ringBufferWritePos,
                   buffer[0].size());

    if (buffer.size() != _channelNum) {
        mainlog->error(L"{} Invalid channel count: expected {}, got {}", _pDeviceId, _channelNum, buffer.size());
        return;
    }

    if (buffer[0].size() != _outBufferSize) {
        mainlog->error(L"{} Invalid chunk length: expected {}, got {}", _pDeviceId, _outBufferSize, buffer[0].size());
        return;
    }

    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);

        if (_ringBufferReadPos == (_ringBufferWritePos + _outBufferSize) % _ringBufferSize) {
            mainlog->warn(L"{} Write overflow!", _pDeviceId);
            return;
        }

        size_t & wp = _ringBufferWritePos;
        for (int ch = 0; ch < _channelNum; ch++) {
            memcpy(
                    &_ringBuffer[ch][wp],
                    buffer[ch].data(),
                    _outBufferSize * sizeof(buffer[ch][0]));
        }
        wp += _outBufferSize;
        if (wp == _ringBufferSize) wp = 0;
    }
}


HRESULT WASAPIOutputEvent::LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient) {
    if (!pRenderClient) {
        return E_INVALIDARG;
    }

    SPDLOG_TRACE_FUNC;

    if (_eventCallback) {
        mainlog->trace(L"{} Pulling data from ASIO side", _pDeviceId);
        _eventCallback();
    }

    BYTE *pData;
    HRESULT hr = pRenderClient->GetBuffer(_outBufferSize, &pData);
    if (FAILED(hr)) {
        mainlog->error(L"{} _pRenderClient->GetBuffer() failed, (0x{0:08X})", _pDeviceId, hr);
        return E_FAIL;
    }

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;
    assert(sampleSize == 2);

    mainlog->trace(L"{} LoadData, rp {} wp {} ringSize {}", _pDeviceId, _ringBufferReadPos, _ringBufferWritePos,
                   _ringBufferSize);

    assert(_ringBufferSize % _outBufferSize == 0);
    bool skipped = false;
    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);
        size_t & rp = _ringBufferReadPos;
        auto out = reinterpret_cast<short *>(pData);

        assert(rp % _outBufferSize == 0);
        if (rp != _ringBufferWritePos) {
            for (int i = 0; i < _outBufferSize; i++) {
                for (unsigned channel = 0; channel < _channelNum; channel++) {
                    *(out++) = _ringBuffer[channel][i + rp];
                }
            }
            rp += _outBufferSize;
            if (rp == _ringBufferSize) rp = 0;
        } else {  // Skip this segment.
            skipped = true;  // Don't log here: we're within mutex
            memset(out, 0, sizeof(short) * _channelNum * _outBufferSize);
        }
    }

    // mainlog->warn may take long, so this logging should be done outside _ringBufferMutex lock.
    if (skipped) {
        mainlog->warn(L"{} [----------] Skipped pushing to wasapi", _pDeviceId);
    }

    pRenderClient->ReleaseBuffer(_outBufferSize, 0);
    return S_OK;
}

/////

DWORD WINAPI WASAPIOutputEvent::playThread(LPVOID pThis) {
    HRESULT hr;

    auto *pDriver = static_cast<WASAPIOutputEvent *>(pThis);
    pDriver->_runningEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    CExitEventSetter setter(pDriver->_runningEvent);

    auto pAudioClient = pDriver->_pAudioClient;
    BYTE *pData = nullptr;

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
        pDriver->LoadData(pRenderClient);
    }

    hr = pAudioClient->Stop(); // Stop playing.
    if (FAILED(hr))
        return -1;

    return 0;
}
//
// Created by whyask37 on 2023-07-01.
//

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
#include "createIAudioClient.h"
#include "../utils/WASAPIUtils.h"
#include "../utils/logger.h"
#include "../utils/raiiUtils.h"

const double m_pi = 3.14159265358979;

const int inBufferSizeMultiplier = 8;

const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

class WASAPIOutputImpl {
public:
    WASAPIOutputImpl(const std::shared_ptr<IMMDevice> &pDevice, int channelNum, int sampleRate, int bufferSizeRequest);

    ~WASAPIOutputImpl();

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    void pushSamples(const std::vector<std::vector<short>> &buffer);

    void registerCallback(std::function<void()> pullCallback) {
        _pullCallback = pullCallback;
    }

private:
    void start();

    void stop();

    static DWORD WINAPI playThread(LPVOID pThis);

private:
    HRESULT LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient);

private:
    int _channelNum;
    int _sampleRate;
    size_t _outBufferSize;

    std::vector<std::vector<short>> _ringBuffer;
    std::mutex _ringBufferMutex;
    size_t _ringBufferSize;
    size_t _ringBufferReadPos = 0;
    size_t _ringBufferWritePos = 0;

    std::shared_ptr<IMMDevice> _pDevice;
    std::shared_ptr<IAudioClient> _pAudioClient;
    std::wstring _pDeviceId;
    WAVEFORMATEXTENSIBLE _waveFormat{};

    HANDLE _stopEvent = nullptr;
    HANDLE _runningEvent = nullptr;

    std::function<void()> _pullCallback;
};

WASAPIOutput::WASAPIOutput(const std::shared_ptr<IMMDevice> &pDevice, int channelNum, int sampleRate,
                           int bufferSizeRequest)
        : _pImpl(std::make_unique<WASAPIOutputImpl>(pDevice, channelNum, sampleRate, bufferSizeRequest)) {}

WASAPIOutput::~WASAPIOutput() = default;

void WASAPIOutput::pushSamples(const std::vector<std::vector<short>> &buffer) {
    _pImpl->pushSamples(buffer);
}

void WASAPIOutput::registerCallback(std::function<void()> pullCallback) {
    _pImpl->registerCallback(pullCallback);
}

////////////

WASAPIOutputImpl::WASAPIOutputImpl(
        const std::shared_ptr<IMMDevice> &pDevice,
        int channelNum,
        int sampleRate,
        int bufferSizeRequest)
        : _pDevice(pDevice), _channelNum(channelNum), _sampleRate(sampleRate) {

    LOGGER_TRACE_FUNC;

    _pDeviceId = getDeviceId(pDevice);

    if (!FindStreamFormat(pDevice, channelNum, sampleRate, bufferSizeRequest, &_waveFormat, &_pAudioClient)) {
        Logger::error(L"Cannot find suitable stream format for output _pDevice (_pDevice ID %s)",
                      getDeviceId(pDevice).c_str());
        throw AppException("FindStreamFormat failed");
    }

    UINT32 bufferSize;
    HRESULT hr = _pAudioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr)) {
        throw AppException("GetBufferSize failed");
    }

    Logger::info(L"WASAPIOutputImpl: %ws - Buffer size %d", _pDeviceId.c_str(), bufferSize);
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

WASAPIOutputImpl::~WASAPIOutputImpl() {
    LOGGER_TRACE_FUNC;

    stop();
    WaitForSingleObject(_runningEvent, INFINITE);
}

void WASAPIOutputImpl::start() {
    LOGGER_TRACE_FUNC;

    // Wait for previous wasapi thread to close
    WaitForSingleObject(_runningEvent, INFINITE);
    if (!_stopEvent) {
        _stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        CreateThread(nullptr, 0, playThread, this, 0, nullptr);
    }
}

void WASAPIOutputImpl::stop() {
    LOGGER_TRACE_FUNC;

    if (_stopEvent) {
        SetEvent(_stopEvent);
        CloseHandle(_stopEvent);
        _stopEvent = nullptr;
    }
}


void WASAPIOutputImpl::pushSamples(const std::vector<std::vector<short>> &buffer) {
    assert (buffer.size() == _channelNum);

    Logger::trace(L"%s pushSamples, rp %d wp %d bufferSize %d", _pDeviceId.c_str(), _ringBufferReadPos, _ringBufferWritePos, buffer[0].size());

    if (buffer.size() != _channelNum) {
        Logger::error("%s Invalid channel count: expected %d, got %d", _pDeviceId.c_str(), _channelNum, buffer.size());
        return;
    }

    if (buffer[0].size() != _outBufferSize) {
        Logger::error("%s Invalid chunk length: expected %d, got %d", _pDeviceId.c_str(), _outBufferSize, buffer[0].size());
        return;
    }

    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);

        if (_ringBufferReadPos == (_ringBufferWritePos + _outBufferSize) % _ringBufferSize) {
            Logger::warn("%s Write overflow!", _pDeviceId.c_str());
            return;
        }

        size_t &wp = _ringBufferWritePos;
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


HRESULT WASAPIOutputImpl::LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient) {
    if (!pRenderClient) {
        return E_INVALIDARG;
    }

    if (_pullCallback) {
        Logger::trace(L"%s Pulling data from ASIO side", _pDeviceId.c_str());
        _pullCallback();
    }

    BYTE *pData;
    pRenderClient->GetBuffer(_outBufferSize, &pData);

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;
    assert(sampleSize == 2);

    Logger::trace(L"%s LoadData, rp %d wp %d ringSize %d", _pDeviceId.c_str(), _ringBufferReadPos, _ringBufferWritePos, _ringBufferSize);

    assert(_ringBufferSize % _outBufferSize == 0);
    bool skipped = false;
    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);
        size_t &rp = _ringBufferReadPos;
        auto out = reinterpret_cast<short *>(pData);


        assert(rp % _outBufferSize == 0);
        if (_ringBufferReadPos != _ringBufferWritePos) {
            auto rpAfterLoad = rp + _outBufferSize;
            for (int i = 0; i < _outBufferSize; i++) {
                for (unsigned channel = 0; channel < _channelNum; channel++) {
                    *(out++) = _ringBuffer[channel][i + rp];
                }
            }
            rp += _outBufferSize;
            if (rp == _ringBufferSize) rp = 0;
        } else {  // Skip this segment.
            skipped = true;  // Don't log here: we're within mutex
            for (int i = 0; i < _outBufferSize; i++) {
                for (unsigned channel = 0; channel < _channelNum; channel++) {
                    *(out++) = 0;
                }
            }
        }
    }
    if (skipped) {
        Logger::warn(L"%s [----------] Skipped pushing to wasapi", _pDeviceId.c_str());
    }

    pRenderClient->ReleaseBuffer(_outBufferSize, 0);
    return S_OK;
}

/////

DWORD WINAPI WASAPIOutputImpl::playThread(LPVOID pThis) {
    HRESULT hr;

    auto *pDriver = static_cast<WASAPIOutputImpl *>(pThis);
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

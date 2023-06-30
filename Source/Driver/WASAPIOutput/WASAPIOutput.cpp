//
// Created by whyask37 on 2023-06-26.
//

const double m_pi = 3.14159265358979;

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
    int t = 0;

    HRESULT LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient);

private:
    int _channelNum;
    int _sampleRate;
    int _outBufferSize;

    std::vector<std::vector<short>> _ringBuffer;
    std::mutex _ringBufferMutex;
    size_t _ringBufferSize;
    size_t _ringBufferReadPos = 0;
    size_t _ringBufferWritePos = 0;

    std::shared_ptr<IMMDevice> _pDevice;
    std::shared_ptr<IAudioClient> _pAudioClient;
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

    auto deviceId = getDeviceId(pDevice);
    Logger::info("WASAPIOutputImpl: %ws - Buffer size %d", deviceId.c_str(), bufferSize);
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


int t = 0;

void WASAPIOutputImpl::pushSamples(const std::vector<std::vector<short>> &buffer2) {
//     TEST code - check whether issue is within pushSamples ~ render stage, or the data fed to pushSamples.
    assert (buffer2.size() == _channelNum);

    std::vector<std::vector<short>> buffer;
    buffer.resize(buffer2.size());
    for (int ch = 0; ch < buffer2.size(); ch++) {
        auto &b = buffer[ch];
        b.resize(buffer2[0].size());
        for (int j = 0; j < b.size(); j++) {
            b[j] = 10000 * sin(2 * m_pi * 440 * (t + j) / 48000);
        }
    }
    t += buffer2[0].size();

    Logger::trace(L"pushSamples, rp %d wp %d bufferSize %d", _ringBufferReadPos, _ringBufferWritePos, buffer[0].size());

    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);

        // Fill in ring buffer
        size_t inBufferSize = buffer[0].size();
        inBufferSize %= _ringBufferSize;  // Possible overflow
        size_t irp = buffer[0].size() - inBufferSize;
        size_t wp = _ringBufferWritePos;

        auto fillToEndSize = min(inBufferSize, _ringBufferSize - wp);
        for (int ch = 0; ch < _channelNum; ch++) {
            memcpy(
                    _ringBuffer[ch].data() + wp,
                    buffer[ch].data() + irp,
                    fillToEndSize * sizeof(short)
            );
        }
        wp += fillToEndSize;
        if (wp == _ringBufferSize) wp = 0;

        auto fillFromFirstSize = inBufferSize - fillToEndSize;
        if (fillFromFirstSize > 0) {
            assert(wp == 0);
            for (int ch = 0; ch < _channelNum; ch++) {
                memcpy(
                        _ringBuffer[ch].data(),
                        buffer[ch].data() + irp + fillToEndSize,
                        fillFromFirstSize * sizeof(short)
                );
            }
            wp = fillFromFirstSize;
        }

        _ringBufferWritePos = wp;
    }
}


HRESULT WASAPIOutputImpl::LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient) {
    if (!pRenderClient) {
        return E_INVALIDARG;
    }

    if (_pullCallback) {
        Logger::trace(L"Pulling data from ASIO side");
        _pullCallback();
    }

    BYTE *pData;
    pRenderClient->GetBuffer(_outBufferSize, &pData);

    UINT32 sampleSize = _waveFormat.Format.wBitsPerSample / 8;
    assert(sampleSize == 2);

    Logger::trace(L"LoadData, rp %d wp %d ringSize %d", _ringBufferReadPos, _ringBufferWritePos, _ringBufferSize);

    assert(_ringBufferSize % _outBufferSize == 0);
    bool skipped = false;
    {
        std::lock_guard<std::mutex> guard(_ringBufferMutex);
        size_t &rp = _ringBufferReadPos;
        short *out = reinterpret_cast<short *>(pData);


        assert(rp % _outBufferSize == 0);
        auto rpAfterLoad = rp + _outBufferSize;
        if (
                (rpAfterLoad == _ringBufferSize && _ringBufferWritePos < rpAfterLoad) ||
                (rpAfterLoad != _ringBufferSize && rpAfterLoad <= _ringBufferWritePos)
                ) {  // Sufficient data to output
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
        Logger::warn(L"[----------] Skipped pushing to wasapi");
    }

    pRenderClient->ReleaseBuffer(_outBufferSize, 0);
    return S_OK;
}

/////

DWORD WINAPI WASAPIOutputImpl::playThread(LPVOID pThis) {
    struct CExitEventSetter {
        HANDLE &m_hEvent;

        explicit CExitEventSetter(WASAPIOutputImpl *pDriver) : m_hEvent(pDriver->_runningEvent) {
            m_hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        }

        ~CExitEventSetter() {
            SetEvent(m_hEvent);
            CloseHandle(m_hEvent);
            m_hEvent = nullptr;
        }
    };

    auto *pDriver = static_cast<WASAPIOutputImpl *>(pThis);

    CExitEventSetter setter(pDriver);
    HRESULT hr = S_OK;

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

//
// Created by whyask37 on 2023-06-26.
//

#define _USE_MATH_DEFINES

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <vector>
#include <mutex>
#include <cassert>
#include <cmath>

#include "WASAPIOutput.h"
#include "createIAudioClient.h"
#include "../WASAPIUtils.h"
#include "../logger.h"
#include "../raiiUtils.h"

const int inBufferSizeMultiplier = 8;

const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

class WASAPIOutputImpl {
public:
    WASAPIOutputImpl(std::shared_ptr<IMMDevice> pDevice, int nChannels, int sampleRate);

    ~WASAPIOutputImpl();

    void pushSamples(short *buffer, int sampleN) {} // TODO: proper implementation

private:
    void start();

    void stop();

    static DWORD WINAPI playThread(LPVOID pThis);

private:
    UINT32 wasapiBufferSize{}; // Size of WASAPI Output Buffer
    int t = 0;

    HRESULT LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient);

private:
    std::shared_ptr<IMMDevice> pDevice;
    int nChannels;
    int sampleRate;

    std::shared_ptr<IMMDevice> device;
    std::shared_ptr<IAudioClient> pAudioClient;
    WAVEFORMATEXTENSIBLE waveFormat{};

    HANDLE stopEvent = nullptr;
    HANDLE runningEvent = nullptr;
};

WASAPIOutput::WASAPIOutput(std::shared_ptr<IMMDevice> pDevice, int nChannels, int sampleRate)
        : _pimpl(std::make_unique<WASAPIOutputImpl>(pDevice, nChannels, sampleRate)) {}

WASAPIOutput::~WASAPIOutput() {}

void WASAPIOutput::pushSamples(short *buffer, int sampleN) {
    _pimpl->pushSamples(buffer, sampleN);
}

////////////

WASAPIOutputImpl::WASAPIOutputImpl(
        std::shared_ptr<IMMDevice> pDevice,
        int nChannels,
        int sampleRate)
        : pDevice(pDevice), nChannels(nChannels), sampleRate(sampleRate) {
    LOGGER_TRACE_FUNC;

    if (!FindStreamFormat(pDevice, nChannels, sampleRate, &waveFormat, &pAudioClient)) {
        Logger::error(L"Cannot find suitable stream format for output device (device ID %s)",
                      getDeviceId(pDevice).c_str());
        throw WASAPIOutputException("FindStreamFormat failed");
    }
    pAudioClient->GetBufferSize(&wasapiBufferSize);

    // TODO: start only when sufficient data is fetched.
    start();
}

WASAPIOutputImpl::~WASAPIOutputImpl() {
    LOGGER_TRACE_FUNC;

    stop();
    WaitForSingleObject(runningEvent, INFINITE);
}

void WASAPIOutputImpl::start() {
    LOGGER_TRACE_FUNC;

    // Wait for previous wasapi thread to close
    WaitForSingleObject(runningEvent, INFINITE);
    if (!stopEvent) {
        stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        CreateThread(nullptr, 0, playThread, this, 0, nullptr);
    }
}

void WASAPIOutputImpl::stop() {
    LOGGER_TRACE_FUNC;

    if (stopEvent) {
        SetEvent(stopEvent);
        CloseHandle(stopEvent);
        stopEvent = nullptr;
    }
}

HRESULT WASAPIOutputImpl::LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient) {
    if (!pRenderClient) {
        return E_INVALIDARG;
    }

    // TODO: implement proper buffer

    BYTE *pData = nullptr;
    pRenderClient->GetBuffer(wasapiBufferSize, &pData);

    UINT32 sampleSize = waveFormat.Format.wBitsPerSample / 8;
    assert(sampleSize == 2);

    // DEBUG: generate sine wave
    unsigned sampleOffset = 0;
    unsigned nextSampleOffset = sampleSize;
    for (int i = 0; i < wasapiBufferSize; i++) {
        for (unsigned channel = 0; channel < nChannels; channel++) {
            short data = (short) (10000 * sin(440 * t * 2 * M_PI / sampleRate));
            memcpy(pData, &data, sampleSize);
            pData += sampleSize;
        }
        t++;
    }

    pRenderClient->ReleaseBuffer(wasapiBufferSize, 0);
    return S_OK;
}

/////

DWORD WINAPI WASAPIOutputImpl::playThread(LPVOID pThis) {
    struct CExitEventSetter {
        HANDLE &m_hEvent;

        explicit CExitEventSetter(WASAPIOutputImpl *pDriver) : m_hEvent(pDriver->runningEvent) {
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

    auto pAudioClient = pDriver->pAudioClient;
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

    HANDLE events[2] = {pDriver->stopEvent, hEvent.get()};
    while ((WaitForMultipleObjects(2, events, FALSE, INFINITE)) ==
           (WAIT_OBJECT_0 + 1)) { // the hEvent is signalled and m_hStopPlayThreadEvent is not
        // Grab the next empty buffer from the audio device.
        pDriver->LoadData(pRenderClient);
    }

    hr = pAudioClient->Stop(); // Stop playing.
    if (FAILED(hr))
        return -1;

    return 0;
}

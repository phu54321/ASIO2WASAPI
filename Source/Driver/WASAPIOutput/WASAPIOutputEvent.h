//
// Created by whyask37 on 2023-07-01.
//

#ifndef ASIO2WASAPI2_WASAPIOUTPUTEVENT_H
#define ASIO2WASAPI2_WASAPIOUTPUTEVENT_H

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <mutex>
#include <memory>
#include <vector>
#include <functional>

#include "WASAPIOutput.h"

class WASAPIOutputEvent : public WASAPIOutput {
public:
    WASAPIOutputEvent(const std::shared_ptr<IMMDevice> &pDevice, int channelNum, int sampleRate, int bufferSizeRequest);

    ~WASAPIOutputEvent();

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    void pushSamples(const std::vector<std::vector<short>> &buffer);

private:
    void start();

    void stop();

    static DWORD WINAPI playThread(LPVOID pThis);

private:
    HRESULT LoadData(const std::shared_ptr<IAudioRenderClient> &pRenderClient);

private:
    int _channelNum;
    int _sampleRate;
    UINT32 _inputBufferSize;
    UINT32 _outputBufferSize;

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
};

#endif //ASIO2WASAPI2_WASAPIOUTPUTEVENT_H

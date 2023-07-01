//
// Created by whyask37 on 2023-07-01.
//

#ifndef ASIO2WASAPI2_WASAPIOUTPUTPUSH_H
#define ASIO2WASAPI2_WASAPIOUTPUTPUSH_H

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <mutex>
#include <memory>
#include <vector>
#include <functional>

#include "WASAPIOutput.h"

class WASAPIOutputPush : public WASAPIOutput {
public:
    WASAPIOutputPush(const std::shared_ptr<IMMDevice> &pDevice, int channelNum, int sampleRate, int bufferSizeRequest);

    ~WASAPIOutputPush();

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    void pushSamples(const std::vector<std::vector<short>> &buffer);

private:
    int _channelNum;
    int _sampleRate;
    size_t _outBufferSize;

    std::shared_ptr<IMMDevice> _pDevice;
    std::shared_ptr<IAudioClient> _pAudioClient;
    std::shared_ptr<IAudioRenderClient> _pRenderClient;
    std::wstring _pDeviceId;
    WAVEFORMATEXTENSIBLE _waveFormat{};
    std::function<void()> _eventCallback;
};

#endif //ASIO2WASAPI2_WASAPIOUTPUTPUSH_H

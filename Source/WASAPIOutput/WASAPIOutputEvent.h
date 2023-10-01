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
#include "tracy/Tracy.hpp"

class WASAPIOutputEvent : public WASAPIOutput {
public:
    WASAPIOutputEvent(const std::shared_ptr<IMMDevice> &pDevice, int channelNum, int sampleRate, int bufferSizeRequest,
                      int ringBufferSizeMultiplier);

    ~WASAPIOutputEvent();

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    void pushSamples(const std::vector<std::vector<int32_t>> &buffer);

    UINT32 getOutputBufferSize() const { return _outputBufferSize; }

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

    std::vector<std::vector<int32_t>> _ringBuffer;
    TracyLockable(std::mutex, _ringBufferMutex);
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

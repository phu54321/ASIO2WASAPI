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
    void pushSamples(const std::vector<std::vector<int32_t>> &buffer);

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

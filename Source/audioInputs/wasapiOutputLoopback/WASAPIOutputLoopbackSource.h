// Copyright (C) 2024 Hyunwoo Park
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
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

//
// Created by whyask37 on 2024-11-27.
//

#ifndef TRGKASIO_WASAPIOUTPUTLOOPBACKSOURCE_H
#define TRGKASIO_WASAPIOUTPUTLOOPBACKSOURCE_H

#include "../AudioSource.h"
#include "../../utils/WASAPIUtils.h"
#include "../../utils/RingBuffer.h"
#include "../../lib/r8brain_free_src/CDSPResampler.h"
#include <string>
#include <memory>

class WASAPIOutputLoopbackSource : public AudioSource {
public:
    WASAPIOutputLoopbackSource(const IMMDevicePtr &device, int channelCount, int sampleRate,
                               bool interceptDefaultOutput);

    ~WASAPIOutputLoopbackSource() override;

    void render(int64_t currentFrame, std::vector<std::vector<int32_t>> *outputBuffer) override;

private:
    void _feedResampledData(int ch, double *input, UINT32 size);

    IMMDevicePtr _pDevice;
    std::wstring _pDeviceId;
    std::wstring _prevOutputDeviceId;

    std::shared_ptr<IAudioCaptureClient> _pAudioCaptureClient;
    std::shared_ptr<IAudioClient> _pAudioClient;
    std::vector<double> _tempBuffer;
    int _sampleRate;
    int _channelCount;
    WAVEFORMATEX *_pwfx = nullptr;
    UINT32 _bufferSize;
    std::vector<std::unique_ptr<r8b::CDSPResampler24>> _resamplers;
    std::vector<RingBuffer<double>> _audioBuffer;
};


#endif //TRGKASIO_WASAPIOUTPUTLOOPBACKSOURCE_H

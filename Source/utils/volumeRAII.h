// Copyright (c) 2023 Park Hyunwoo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef XANTHINE_VOLUMERAII_H
#define XANTHINE_VOLUMERAII_H

#include <endpointvolume.h>
#include <memory>
#include "WASAPIUtils.h"

class VolumeRAII {
public:
    VolumeRAII(IMMDevicePtr device);

    ~VolumeRAII();

    bool get(bool *currentMute, float *currentVolume) const;

    void setMute(bool mute);

    void setVolume(float newVolume);

private:
    std::shared_ptr<IAudioEndpointVolume> _endpointVolume = nullptr;
    float _oldVolume = 0.0f;
    bool _oldMute = false;
    bool _active = true;
};

#endif // XANTHINE_VOLUMERAII_H

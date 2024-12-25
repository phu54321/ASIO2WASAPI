// Copyright (c) 2023 Park Hyunwoo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "volumeRAII.h"
#include "WASAPIUtils.h"
#include "raiiUtils.h"
#include "logger.h"

VolumeRAII::VolumeRAII(IMMDevicePtr device) {
    HRESULT hr;

    IAudioEndpointVolume *raw_endpointVolume;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL,
                          reinterpret_cast<void **>(&raw_endpointVolume));
    if (FAILED(hr)) {
        mainlog->warn("Cannot init IAudioEndpointVolume: {:08X}", hr);
        return;
    }
    auto endpointVolume = make_autorelease(raw_endpointVolume);
    _endpointVolume = endpointVolume;

    if (!get(&_oldMute, &_oldVolume)) {
        mainlog->warn("Cannot get previous mute/volume status. Stall any activity happening in this device.");
        _endpointVolume = nullptr;
    }
}

bool VolumeRAII::get(bool *currentMute, float *currentVolume) const {
    if (!_endpointVolume) return false;

    HRESULT hr;
    bool allSucceeded = true;

    if (currentMute) {
        BOOL mute;
        hr = _endpointVolume->GetMute(&mute);
        if (FAILED(hr)) {
            mainlog->warn("GetMute failed: {:08X}", hr);
            allSucceeded = false;
        } else {
            *currentMute = !!mute;
        }
    }

    if (currentVolume) {
        float vol;
        hr = _endpointVolume->GetMasterVolumeLevelScalar(&vol);
        if (FAILED(hr)) {
            mainlog->warn("GetMasterVolumeLevelScalar failed: {:08X}", hr);
            allSucceeded = false;
        } else {
            *currentVolume = vol;
        }
    }

    return allSucceeded;
}

void VolumeRAII::setVolume(float newVolume) {
    if (!_endpointVolume) return;

    HRESULT hr;

    mainlog->debug("SetMasterVolumeLevelScalar: {}", newVolume);
    hr = _endpointVolume->SetMasterVolumeLevelScalar(newVolume, nullptr);
    if (FAILED(hr)) {
        mainlog->warn("SetMasterVolumeLevelScalar failed: {:08X}", hr);
    }
}

void VolumeRAII::setMute(bool mute) {
    if (!_endpointVolume) return;

    HRESULT hr;

    mainlog->debug("SetMute: {}", mute);
    hr = _endpointVolume->SetMute(mute, nullptr);
    if (FAILED(hr)) {
        mainlog->warn("SetMute failed: {:08X}", hr);
    }
}

VolumeRAII::~VolumeRAII() {
    if (_endpointVolume) {
        setMute(_oldMute);
        setVolume(_oldVolume);
    }
}

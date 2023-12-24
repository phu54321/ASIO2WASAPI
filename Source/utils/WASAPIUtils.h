// Copyright (C) 2023 Hyun Woo Park
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
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.


#ifndef TRGKASIO_WASAPIUTILS_H
#define TRGKASIO_WASAPIUTILS_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mmdeviceapi.h>

using IMMDevicePtr = std::shared_ptr<IMMDevice>;

/**
 * Iterate available output WASAPI audio endpoints.
 * @param cb callback. called with `shared_ptr<IMMDevice>`.
 *           if this returns false, loop ends.
 * @return if loop completed without without interruptions, return true.
 */
bool iterateAudioEndPoints(const std::function<bool(IMMDevicePtr pMMDevice)> &cb);

/**
 * Get default output device
 * @return nullptr if none exists.
 */
IMMDevicePtr getDefaultOutputDevice();

/**
 * Get devices in vector form
 * @return
 */
std::vector<IMMDevicePtr> getIMMDeviceList();

/**
 * Get _pDevice ID of IMMDevice
 * @param pDevice IMMDevice
 * @return _pDevice ID
 */
std::wstring getDeviceId(const IMMDevicePtr &pDevice);

/**
 * Get friendly name of IMMDevice
 * @param pDevice IMMDevice
 * @return friendly name
 */
std::wstring getDeviceFriendlyName(const IMMDevicePtr &pDevice);


#endif //TRGKASIO_WASAPIUTILS_H

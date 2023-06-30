//
// Created by whyask37 on 2023-06-26.
//

#ifndef ASIO2WASAPI2_WASAPIUTILS_H
#define ASIO2WASAPI2_WASAPIUTILS_H

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
bool iterateAudioEndPoints(std::function<bool(IMMDevicePtr pMMDevice)> cb);

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


#endif //ASIO2WASAPI2_WASAPIUTILS_H

//
// Created by whyask37 on 2023-06-26.
//

#ifndef ASIO2WASAPI2_WASAPIUTILS_H
#define ASIO2WASAPI2_WASAPIUTILS_H

#include <string>
#include <memory>
#include <functional>
#include <mmdeviceapi.h>

/**
 * Iterate available output WASAPI audio endpoints.
 * @param cb callback. called with `shared_ptr<IMMDevice>`.
 *           if this returns false, loop ends.
 * @return if loop completed without without interruptions, return true.
 */
bool iterateAudioEndPoints(std::function<bool(std::shared_ptr<IMMDevice> pMMDevice)> cb);

/**
 * Get device ID of IMMDevice
 * @param pDevice IMMDevice
 * @return device ID
 */
std::wstring getDeviceId(const std::shared_ptr<IMMDevice> &pDevice);

/**
 * Get friendly name of IMMDevice
 * @param pDevice IMMDevice
 * @return friendly name
 */
std::wstring getDeviceFriendlyName(const std::shared_ptr<IMMDevice> &pDevice);

/**
 * Get device from device ID (wstring)
 * @param deviceId
 * @return matching device. nullptr if one doesn't exist.
 */
std::shared_ptr<IMMDevice> getDeviceFromId(const std::wstring &deviceId);

#endif //ASIO2WASAPI2_WASAPIUTILS_H

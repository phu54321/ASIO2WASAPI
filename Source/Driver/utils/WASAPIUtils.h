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
 * Get _pDevice ID of IMMDevice
 * @param pDevice IMMDevice
 * @return _pDevice ID
 */
std::wstring getDeviceId(const std::shared_ptr<IMMDevice> &pDevice);

/**
 * Get friendly name of IMMDevice
 * @param pDevice IMMDevice
 * @return friendly name
 */
std::wstring getDeviceFriendlyName(const std::shared_ptr<IMMDevice> &pDevice);

/**
 * Get _pDevice from _pDevice ID (wstring)
 * @param deviceId
 * @return matching _pDevice. nullptr if one doesn't exist.
 */
std::shared_ptr<IMMDevice> getDeviceFromId(const std::wstring &deviceId);

#endif //ASIO2WASAPI2_WASAPIUTILS_H

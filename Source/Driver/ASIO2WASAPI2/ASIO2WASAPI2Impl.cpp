/*  ASIO2WASAPI2 Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of ASIO2WASAPI2.

    ASIO2WASAPI2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIO2WASAPI2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIO2WASAPI2; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <cstring>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "ASIO2WASAPI2Impl.h"
#include "../resource.h"
#include "../utils/json.hpp"
#include "../WASAPIOutput/createIAudioClient.h"
#include "../utils/raiiUtils.h"
#include "RunningState.h"
#include "PreparedState.h"
#include "../utils/logger.h"
#include "../utils/hexdump.h"

using json = nlohmann::json;

static const ASIOSampleType sampleType = ASIOSTInt32LSB;

ASIO2WASAPI2Impl::ASIO2WASAPI2Impl(void *sysRef)
        : _settings(loadDriverSettings()) {
    SPDLOG_TRACE_FUNC;

    mainlog->info("Starting ASIO2WASAPI2...");

    CoInitialize(nullptr);

    auto &targetDeviceIdList = _settings.deviceIdList;

    auto defaultDeviceId = getDeviceId(getDefaultOutputDevice());
    auto devices = getIMMDeviceList();

    std::vector<std::wstring> deviceIdList;
    std::transform(devices.begin(), devices.end(), std::back_inserter(deviceIdList), getDeviceId);
    std::vector<std::wstring> friendlyNameList;
    std::transform(devices.begin(), devices.end(), std::back_inserter(friendlyNameList), getDeviceFriendlyName);


    mainlog->info("Will find these {} devices", targetDeviceIdList.size());
    for (int i = 0; i < targetDeviceIdList.size(); i++) {
        if (targetDeviceIdList[i].empty() || targetDeviceIdList[i] == L"(default)") {
            targetDeviceIdList[i] = defaultDeviceId;
            mainlog->info(L" - Target #{:02d}: default output device {}", i, defaultDeviceId);
        } else {
            mainlog->info(L" - Target #{:02d}: {}", i, targetDeviceIdList[i]);
        }
    }

    mainlog->info("Enumerating devices - Total {} device found", devices.size());
    std::map<std::wstring, IMMDevicePtr> deviceMap;
    for (int i = 0; i < devices.size(); i++) {
        mainlog->info(L" - Device #{:02d}: {} {}", i, friendlyNameList[i], deviceIdList[i]);
        for (const auto &id: _settings.deviceIdList) {
            auto &device = devices[i];
            if (id == deviceIdList[i] || id == friendlyNameList[i]) {
                mainlog->info("   : Matched");
                deviceMap[id] = device;
                break;
            }
        }
    }

    // Put _pDeviceList with order of _settings.deviceIdList
    mainlog->info("Total {} device matched", deviceMap.size());
    for (const auto &id: _settings.deviceIdList) {
        auto it = deviceMap.find(id);
        if (it != deviceMap.end()) {
            _pDeviceList.push_back(it->second);
        }
    }


    if (_pDeviceList.empty()) {
        throw AppException("No target device(s) found...");
    }
}

ASIO2WASAPI2Impl::~ASIO2WASAPI2Impl() {
    SPDLOG_TRACE_FUNC;

    mainlog->info("Stopping ASIO2WASAPI2...");
    mainlog->flush();

    stop();
    disposeBuffers();
}



/////

ASIOError ASIO2WASAPI2Impl::getChannels(long *numInputChannels, long *numOutputChannels) {
    SPDLOG_TRACE_FUNC;

    if (numInputChannels) *numInputChannels = 0;
    if (numOutputChannels) *numOutputChannels = _settings.nChannels;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::getSampleRate(ASIOSampleRate *sampleRate) {
    SPDLOG_TRACE_FUNC;

    if (!sampleRate) return ASE_InvalidParameter;
    *sampleRate = _settings.nSampleRate;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::canSampleRate(ASIOSampleRate sampleRate) {
    SPDLOG_TRACE_FUNC;

    int nSampleRate = static_cast<int>(sampleRate);
    for (auto &device: _pDeviceList) {
        if (!FindStreamFormat(device, _settings.nChannels, nSampleRate)) return ASE_NoClock;
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::setSampleRate(ASIOSampleRate sampleRate) {
    SPDLOG_TRACE_FUNC;

    mainlog->debug("setSampleRate: {} ( {} )", sampleRate, hexdump(&sampleRate, sizeof(sampleRate)));
    if (sampleRate == _settings.nSampleRate) return ASE_OK;

    auto err = canSampleRate(sampleRate);
    if (err != ASE_OK) {
        mainlog->debug(L"canSampleRate: {} (error)", err);
        return err;
    }

    _settings.nSampleRate = (int) sampleRate;

    if (_preparedState) {
        _preparedState->requestReset();
    }

    return ASE_OK;
}

///////////////

// all buffer sizes are in frames

static const char *knownChannelNames[] =
        {
                "Front left",
                "Front right",
                "Front center",
                "Low frequency",
                "Back left",
                "Back right",
                "Front left of center",
                "Front right of center",
                "Back center",
                "Side left",
                "Side right",
        };

ASIOError ASIO2WASAPI2Impl::getChannelInfo(ASIOChannelInfo *info) {
    SPDLOG_TRACE_FUNC;

    if (info->isInput) return ASE_InvalidParameter;
    if (info->channel < 0 || info->channel >= _settings.nChannels) return ASE_InvalidParameter;

    info->type = sampleType;
    info->channelGroup = 0;
    info->isActive = _preparedState ? ASIOTrue : ASIOFalse;

    strcpy_s(info->name,
             (info->channel < sizeof(knownChannelNames) / sizeof(knownChannelNames[0]))
             ? knownChannelNames[info->channel]
             : "Unknown");

    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::createBuffers(
        ASIOBufferInfo *bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks *callbacks) {

    SPDLOG_TRACE_FUNC;

    // Check parameters
    if (!callbacks) return ASE_InvalidParameter;
    if (numChannels < 0 || numChannels > _settings.nChannels) return ASE_InvalidParameter;
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= _settings.nChannels)
            return ASE_InvalidMode;
    }

    // dispose exiting buffers
    disposeBuffers();

    // Allocate!
    _settings.bufferSize = bufferSize;
    _preparedState = std::make_shared<PreparedState>(_pDeviceList, _settings, callbacks);
    _preparedState->InitASIOBufferInfo(bufferInfos, numChannels);

    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::disposeBuffers() {
    SPDLOG_TRACE_FUNC;
    stop();

    // wait for the play thread to finish
    _preparedState = nullptr;
    return ASE_OK;
}



////////////

ASIOError ASIO2WASAPI2Impl::start() {
    SPDLOG_TRACE_FUNC;

    if (!_preparedState) return ASE_NotPresent;
    return _preparedState->start() ? ASE_OK : ASE_HWMalfunction;
}

ASIOError ASIO2WASAPI2Impl::outputReady() {
    SPDLOG_TRACE_FUNC;
    if (_preparedState) {
        _preparedState->outputReady();
    }
    return ASE_OK;
}


ASIOError ASIO2WASAPI2Impl::stop() {
    SPDLOG_TRACE_FUNC;

    if (_preparedState) {
        _preparedState->stop();
    }
    return ASE_OK;
}


////////
// auxillary functions


ASIOError ASIO2WASAPI2Impl::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) {
    if (!_preparedState)
        return ASE_NotPresent;

    return _preparedState->getSamplePosition(sPos, tStamp);
}

ASIOError ASIO2WASAPI2Impl::getLatencies(long *_inputLatency, long *_outputLatency) {
    if (!_preparedState)
        return ASE_NotPresent;
    if (_inputLatency)
        *_inputLatency = _settings.bufferSize;
    if (_outputLatency)
        *_outputLatency = 2 * _settings.bufferSize;
    return ASE_OK;
}

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

#include <cstdio>
#include <cstring>
#include <cmath>

#include <Windows.h>
#include <timeapi.h>
#include "ASIO2WASAPI2Impl.h"
#include "../resource.h"
#include "../utils/logger.h"
#include "../utils/json.hpp"
#include "../WASAPIOutput/createIAudioClient.h"
#include "../utils/WASAPIUtils.h"
#include "../utils/raiiUtils.h"
#include "RunningState.h"
#include "PreparedState.h"

using json = nlohmann::json;

static const ASIOSampleType sampleType = ASIOSTInt16LSB;

extern HINSTANCE g_hinstDLL;

ASIO2WASAPI2Impl::ASIO2WASAPI2Impl(void *sysRef) {
    LOGGER_TRACE_FUNC;

    settingsReadFromRegistry();

    CoInitialize(nullptr);

    auto pDevice = getDeviceFromId(_settings.deviceId);
    if (!pDevice) { // id not found
        throw AppException("Target device not found");
    }
    _pDevice = pDevice;
}

ASIO2WASAPI2Impl::~ASIO2WASAPI2Impl() {
    LOGGER_TRACE_FUNC;

    stop();
    disposeBuffers();
}



/////

ASIOError ASIO2WASAPI2Impl::getChannels(long *numInputChannels, long *numOutputChannels) {
    LOGGER_TRACE_FUNC;

    if (numInputChannels) *numInputChannels = 0;
    if (numOutputChannels) *numOutputChannels = _settings.nChannels;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::getSampleRate(ASIOSampleRate *sampleRate) {
    LOGGER_TRACE_FUNC;

    if (!sampleRate) return ASE_InvalidParameter;
    *sampleRate = _settings.nSampleRate;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::canSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    int nSampleRate = static_cast<int>(sampleRate);
    return FindStreamFormat(_pDevice, _settings.nChannels, nSampleRate) ? ASE_OK : ASE_NoClock;
}

ASIOError ASIO2WASAPI2Impl::setSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    Logger::debug(L"setSampleRate: %f", sampleRate);
    if (sampleRate == _settings.nSampleRate) return ASE_OK;

    auto err = canSampleRate(sampleRate);
    if (err != ASE_OK) {
        Logger::debug(L"canSampleRate: %d (error)", err);
        return err;
    }

    int nPrevSampleRate = _settings.nSampleRate;
    _settings.nSampleRate = (int) sampleRate;
    settingsWriteToRegistry();  // new nSampleRate used here

    if (_preparedState) {
        _settings.nSampleRate = nPrevSampleRate;
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
    LOGGER_TRACE_FUNC;

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

    LOGGER_TRACE_FUNC;

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
    _preparedState = std::make_shared<PreparedState>(_pDevice, _settings, callbacks);
    _preparedState->InitASIOBufferInfo(bufferInfos, numChannels);

    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::disposeBuffers() {
    LOGGER_TRACE_FUNC;
    stop();

    // wait for the play thread to finish
    _preparedState = nullptr;
    return ASE_OK;
}



////////////

ASIOError ASIO2WASAPI2Impl::start() {
    LOGGER_TRACE_FUNC;

    if (!_preparedState) return ASE_NotPresent;
    return _preparedState->start() ? ASE_OK : ASE_HWMalfunction;
}

ASIOError ASIO2WASAPI2Impl::outputReady() {
    LOGGER_TRACE_FUNC;
    if (_preparedState) {
        _preparedState->outputReady();
    }
    return ASE_OK;
}


ASIOError ASIO2WASAPI2Impl::stop() {
    LOGGER_TRACE_FUNC;

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

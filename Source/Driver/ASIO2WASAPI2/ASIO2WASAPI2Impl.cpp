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

using json = nlohmann::json;

static const uint64_t twoRaisedTo32 = UINT64_C(4294967296);
static const ASIOSampleType sampleType = ASIOSTInt16LSB;

extern HINSTANCE g_hinstDLL;

ASIO2WASAPI2Impl::ASIO2WASAPI2Impl(void *sysRef) {
    LOGGER_TRACE_FUNC;

    settingsReadFromRegistry();

    m_hAppWindowHandle = (HWND) sysRef;
    CoInitialize(nullptr);

    bool bDeviceFound = false;
    int deviceIndex = 0;
    const auto &targetDeviceId = m_settings.deviceId;
    Logger::debug(L"Listing devices... (searching %ws)", targetDeviceId.c_str());
    if (!targetDeviceId.empty()) {
        iterateAudioEndPoints([&](auto pMMDevice) {
            auto deviceId = getDeviceId(pMMDevice);
            auto friendlyName = getDeviceFriendlyName(pMMDevice);
            Logger::debug(L" - Device #%d: %ws (%ws)", deviceIndex++, friendlyName.c_str(), deviceId.c_str());
            if (deviceId == targetDeviceId) {
                Logger::info(L"Found the device");
                m_pDevice = pMMDevice;
                bDeviceFound = true;
                return false;
            }
            return true;
        });
    }

    if (!bDeviceFound) { // id not found
        Logger::error(L"Target device not found: %s", targetDeviceId.c_str());
        throw AppException("Target device not found");
    }
}

ASIO2WASAPI2Impl::~ASIO2WASAPI2Impl() {
    LOGGER_TRACE_FUNC;

    stop();
    disposeBuffers();
}


ASIOError ASIO2WASAPI2Impl::outputReady() {
    LOGGER_TRACE_FUNC;

    // TODO: use this.
    return ASE_NotPresent;
}

/////

ASIOError ASIO2WASAPI2Impl::getChannels(long *numInputChannels, long *numOutputChannels) {
    LOGGER_TRACE_FUNC;

    if (numInputChannels) *numInputChannels = 0;
    if (numOutputChannels) *numOutputChannels = m_settings.nChannels;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::getSampleRate(ASIOSampleRate *sampleRate) {
    LOGGER_TRACE_FUNC;

    if (!sampleRate) return ASE_InvalidParameter;
    *sampleRate = m_settings.nSampleRate;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::canSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    int nSampleRate = static_cast<int>(sampleRate);
    return FindStreamFormat(m_pDevice, m_settings.nChannels, nSampleRate) ? ASE_OK : ASE_NoClock;
}

ASIOError ASIO2WASAPI2Impl::setSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    Logger::debug(L"setSampleRate: %f", sampleRate);
    if (sampleRate == m_settings.nSampleRate) return ASE_OK;

    auto err = canSampleRate(sampleRate);
    if (err != ASE_OK) {
        Logger::debug(L"canSampleRate: %d (error)", err);
        return err;
    }

    int nPrevSampleRate = m_settings.nSampleRate;
    m_settings.nSampleRate = (int) sampleRate;
    settingsWriteToRegistry();  // new nSampleRate used here
    if (m_callbacks) { // ask the host ro reset us
        m_settings.nSampleRate = nPrevSampleRate;
        m_callbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
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
    if (info->channel < 0 || info->channel >= m_settings.nChannels) return ASE_InvalidParameter;

    info->type = sampleType;
    info->channelGroup = 0;
    info->isActive = (!m_buffers[0].empty()) ? ASIOTrue : ASIOFalse;

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
    if (numChannels < 0 || numChannels > m_settings.nChannels) return ASE_InvalidParameter;
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= m_settings.nChannels)
            return ASE_InvalidMode;
    }

    // TODO: put buffer in some other RAII-ed struct
    // dispose exiting buffers
    disposeBuffers();

    // Allocate!
    m_bufferSize = bufferSize;
    m_callbacks = callbacks;
    m_buffers[0].resize(m_settings.nChannels);
    m_buffers[1].resize(m_settings.nChannels);
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        m_buffers[0].at(info.channelNum).resize(bufferSize);
        m_buffers[1].at(info.channelNum).resize(bufferSize);
        info.buffers[0] = m_buffers[0].at(info.channelNum).data();
        info.buffers[1] = m_buffers[0].at(info.channelNum).data();
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::disposeBuffers() {
    LOGGER_TRACE_FUNC;
    stop();

    // wait for the play thread to finish
    m_output = nullptr;
    m_buffers[0].clear();
    m_buffers[1].clear();
    return ASE_OK;
}



////////////

ASIOError ASIO2WASAPI2Impl::start() {
    LOGGER_TRACE_FUNC;

    if (!m_callbacks)
        return ASE_NotPresent;
    if (m_output)
        return ASE_OK; // we are already playing

    // make sure the previous play thread exited
    m_samplePosition = 0;
    m_output = std::make_unique<WASAPIOutput>(
            m_pDevice,
            m_settings.nChannels,
            m_settings.nSampleRate,
            m_bufferSize);

    // TODO: make enqueue thread

    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::stop() {
    LOGGER_TRACE_FUNC;

    if (!m_output) return ASE_OK; // we already stopped
    m_output = nullptr;
    return ASE_OK;
}


////////
// auxillary functions


ASIOError ASIO2WASAPI2Impl::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) {
    if (!m_callbacks)
        return ASE_NotPresent;

    if (tStamp) {
        tStamp->lo = m_theSystemTime.lo;
        tStamp->hi = m_theSystemTime.hi;
    }
    if (sPos) {
        if (m_samplePosition >= twoRaisedTo32) {
            sPos->hi = (unsigned long) (m_samplePosition / twoRaisedTo32);
            sPos->lo = (unsigned long) (m_samplePosition - (sPos->hi * twoRaisedTo32));
        } else {
            sPos->hi = 0;
            sPos->lo = (unsigned long) m_samplePosition;
        }
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI2Impl::getLatencies(long *_inputLatency, long *_outputLatency) {
    if (!m_callbacks)
        return ASE_NotPresent;
    if (_inputLatency)
        *_inputLatency = m_bufferSize;
    if (_outputLatency)
        *_outputLatency = 2 * m_bufferSize;
    return ASE_OK;
}

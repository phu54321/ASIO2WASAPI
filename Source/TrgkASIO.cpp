// Copyright (C) 2023 Hyunwoo Park
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
//

#include <Windows.h>
#include "TrgkASIO.h"
#include "TrgkASIOImpl.h"
#include "utils/logger.h"
#include <tracy/Tracy.hpp>
#include <spdlog/spdlog.h>
#include <shellapi.h>
#include <spdlog/fmt/fmt.h>

const CLSID CLSID_TRGKASIO_DRIVER = {0xe3226090, 0x473d, 0x4cc9, {0x83, 0x60, 0xe1, 0x23, 0xeb, 0x9e, 0xf8, 0x47}};

void enableHighPrecisionTimer() {
    // For Windows 11: apps require this code to
    // get 1ms timer accuracy when backgrounded.
    PROCESS_POWER_THROTTLING_STATE PowerThrottling;
    RtlZeroMemory(&PowerThrottling, sizeof(PowerThrottling));
    PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    PowerThrottling.StateMask = 0;
    if (SetProcessInformation(GetCurrentProcess(),
                              ProcessPowerThrottling,
                              &PowerThrottling,
                              sizeof(PowerThrottling)) == 0) {
        auto err = GetLastError();
        TCHAR *message = nullptr;
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                      nullptr,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (TCHAR *) &message,
                      0,
                      nullptr);
        mainlog->error(
                TEXT("SetProcessInformation(~PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION) failed: 0x{:08X} ({})"),
                err, message);
        LocalFree(message);
    } else {
        mainlog->info("High-precision timeSetEvent enabled");
    }
}

TrgkASIO::TrgkASIO(LPUNKNOWN pUnk, HRESULT *phr)
        : CUnknown(TEXT("TrgkASIO"), pUnk, phr) {
    // Note: this code is called on DLL_PROCESS_ATTACH, so
    // this shouldn't really contain any kernel-interacting code.

    // Move additional kernal interacting code to `TrgkASIO::init`.
}

TrgkASIO::~TrgkASIO() = default;

/*  ASIO driver interface implementation
 */

CUnknown *TrgkASIO::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr) {
    return static_cast<CUnknown *>(new TrgkASIO(pUnk, phr));
}

STDMETHODIMP TrgkASIO::NonDelegatingQueryInterface(REFIID riid, void **ppv) {
    if (riid == CLSID_TRGKASIO_DRIVER) {
        return GetInterface(this, ppv);
    }
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}


void TrgkASIO::getDriverName(char *name) {
    strcpy_s(name, 32, "TrgkASIO");
}

long TrgkASIO::getDriverVersion() {
    return 1;
}

void TrgkASIO::getErrorMessage(char *s) {
    // TODO: maybe add useful message
    s[0] = 0;
}

ASIOError TrgkASIO::future(long selector, void *opt) {
    // none of the optional features are present
    return ASE_NotPresent;
}


ASIOBool TrgkASIO::init(void *sysRef) {
    ZoneScoped;

    initMainLog();
    mainlog->info("Starting trgkASIO");
    enableHighPrecisionTimer();

    if (_pImpl) return true;
    try {
        _pImpl = std::make_unique<TrgkASIOImpl>(sysRef);
        return true;
    } catch (AppException &e) {
        // Swallow here...
        auto string = fmt::format("trgkASIOImpl constructor failed: {}", e.what());
        mainlog->error(string);
        MessageBoxA((HWND) sysRef, string.c_str(), "Error", MB_OK);
        return false;
    }
}

///////////

ASIOError TrgkASIO::getSampleRate(ASIOSampleRate *sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    auto ret = _pImpl->getSampleRate(sampleRate);
    mainlog->debug(L"getSampleRate: {:.1f}", *sampleRate);
    return ret;
}

ASIOError TrgkASIO::canSampleRate(ASIOSampleRate sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    auto ret = _pImpl->canSampleRate(sampleRate);
    mainlog->debug(L"camSampleRate: {:.1f} - {}", sampleRate, ret);
    return ret;
}

ASIOError TrgkASIO::setSampleRate(ASIOSampleRate sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    if (sampleRate == 0) {
        mainlog->debug(L"setSampleRate: 0 (external sync) - we don't have external clock, so ignoring");
        return ASE_NoClock;
    }

    return _pImpl->setSampleRate(sampleRate);
}

///////////////

// all buffer sizes are in frames
ASIOError TrgkASIO::getBufferSize(long *minSize, long *maxSize,
                                  long *preferredSize, long *granularity) {
    if (!_pImpl) return ASE_NotPresent;
    if (minSize) *minSize = 64;
    if (maxSize) *maxSize = 1024;
    if (preferredSize) *preferredSize = 64;
    if (granularity) *granularity = -1;
    return ASE_OK;
}

ASIOError TrgkASIO::getChannelInfo(ASIOChannelInfo *info) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getChannelInfo(info);
}

ASIOError TrgkASIO::createBuffers(
        ASIOBufferInfo *bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks *callbacks) {

    ZoneScoped;

    if (!_pImpl) {
        mainlog->debug("createBuffers: ASE_NotPresent");
        return ASE_NotPresent;
    }
    return _pImpl->createBuffers(bufferInfos, numChannels, bufferSize, callbacks);
}

ASIOError TrgkASIO::disposeBuffers() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->disposeBuffers();
}



////////////

ASIOError TrgkASIO::start() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->start();
}

ASIOError TrgkASIO::stop() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->stop();
}

ASIOError TrgkASIO::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getSamplePosition(sPos, tStamp);
}

ASIOError TrgkASIO::outputReady() {
    if (_pImpl) {
        _pImpl->outputReady();
    }
    return ASE_OK;
}

////////
// auxillary functions

ASIOError TrgkASIO::getClockSources(ASIOClockSource *clocks, long *numSources) {
    if (!numSources || *numSources == 0)
        return ASE_OK;
    clocks->index = 0;
    clocks->associatedChannel = -1;
    clocks->associatedGroup = -1;
    clocks->isCurrentSource = ASIOTrue;
    strcpy_s(clocks->name, "Internal clock");
    *numSources = 1;
    return ASE_OK;
}

ASIOError TrgkASIO::setClockSource(long index) {
    ZoneScoped;

    return (index == 0) ? ASE_OK : ASE_NotPresent;
}


ASIOError TrgkASIO::getLatencies(long *_inputLatency, long *_outputLatency) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getLatencies(_inputLatency, _outputLatency);
}

ASIOError TrgkASIO::controlPanel() {
    ShellExecute(
            nullptr, nullptr,
            L"https://github.com/phu54321/trgkASIO",
            nullptr, nullptr, SW_SHOW);
    return ASE_OK;
}

ASIOError TrgkASIO::getChannels(long *numInputChannels, long *numOutputChannels) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getChannels(numInputChannels, numOutputChannels);
}

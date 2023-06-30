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

#include <Windows.h>
#include "ASIO2WASAPI2.h"
#include "ASIO2WASAPI2Impl.h"
#include "../utils/logger.h"


CLSID CLSID_ASIO2WASAPI2_DRIVER = {0xe3226090, 0x473d, 0x4cc9, {0x83, 0x60, 0xe1, 0x23, 0xeb, 0x9e, 0xf8, 0x47}};

ASIO2WASAPI2::ASIO2WASAPI2(LPUNKNOWN pUnk, HRESULT *phr)
        : CUnknown(TEXT("ASIO2WASAPI2"), pUnk, phr) {}

ASIO2WASAPI2::~ASIO2WASAPI2() = default;

/*  ASIO driver interface implementation
 */

void ASIO2WASAPI2::getDriverName(char *name) {
    strcpy_s(name, 32, "ASIO2WASAPI2");
}

long ASIO2WASAPI2::getDriverVersion() {
    return 1;
}

void ASIO2WASAPI2::getErrorMessage(char *s) {
    // TODO: maybe add useful message
    s[0] = 0;
}

ASIOError ASIO2WASAPI2::future(long selector, void *opt) {
    // none of the optional features are present
    return ASE_NotPresent;
}


ASIOBool ASIO2WASAPI2::init(void *sysRef) {
    LOGGER_TRACE_FUNC;

    if (_pImpl) return true;
    try {
        _pImpl = std::make_unique<ASIO2WASAPI2Impl>(sysRef);
        return true;
    } catch (AppException &e) {
        // Swallow here...
        Logger::error("ASIO2WASAPI2Impl constructor failed: %s", e.what());
        return false;
    }
}

///////////

ASIOError ASIO2WASAPI2::getSampleRate(ASIOSampleRate *sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getSampleRate(sampleRate);
}

ASIOError ASIO2WASAPI2::canSampleRate(ASIOSampleRate sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->canSampleRate(sampleRate);
}

ASIOError ASIO2WASAPI2::setSampleRate(ASIOSampleRate sampleRate) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->setSampleRate(sampleRate);
}

///////////////

// all buffer sizes are in frames
ASIOError ASIO2WASAPI2::getBufferSize(long *minSize, long *maxSize,
                                      long *preferredSize, long *granularity) {
    if (!_pImpl) return ASE_NotPresent;
    if (minSize) *minSize = 32;
    if (maxSize) *maxSize = 1024;
    if (preferredSize) *preferredSize = 1024;
    if (granularity) *granularity = -1;
    return ASE_OK;
}

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

ASIOError ASIO2WASAPI2::getChannelInfo(ASIOChannelInfo *info) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getChannelInfo(info);
}

ASIOError ASIO2WASAPI2::createBuffers(
        ASIOBufferInfo *bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks *callbacks) {

    LOGGER_TRACE_FUNC;

    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->createBuffers(bufferInfos, numChannels, bufferSize, callbacks);
}

ASIOError ASIO2WASAPI2::disposeBuffers() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->disposeBuffers();
}



////////////

ASIOError ASIO2WASAPI2::start() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->start();
}

ASIOError ASIO2WASAPI2::stop() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->stop();
}

ASIOError ASIO2WASAPI2::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getSamplePosition(sPos, tStamp);
}

ASIOError ASIO2WASAPI2::outputReady() {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->outputReady();
}

////////
// auxillary functions

ASIOError ASIO2WASAPI2::getClockSources(ASIOClockSource *clocks, long *numSources) {
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

ASIOError ASIO2WASAPI2::setClockSource(long index) {
    LOGGER_TRACE_FUNC;

    return (index == 0) ? ASE_OK : ASE_NotPresent;
}


ASIOError ASIO2WASAPI2::getLatencies(long *_inputLatency, long *_outputLatency) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getLatencies(_inputLatency, _outputLatency);
}

ASIOError ASIO2WASAPI2::controlPanel() {
    return ASE_NotPresent;
}

ASIOError ASIO2WASAPI2::getChannels(long *numInputChannels, long *numOutputChannels) {
    if (!_pImpl) return ASE_NotPresent;
    return _pImpl->getChannels(numInputChannels, numOutputChannels);
}

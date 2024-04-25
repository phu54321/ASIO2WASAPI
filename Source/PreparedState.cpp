/*  TrgkASIO Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of TrgkASIO.

    TrgkASIO is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    TrgkASIO is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TrgkASIO; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "PreparedState.h"
#include "RunningState.h"
#include "utils/WASAPIUtils.h"
#include "utils/logger.h"
#include <spdlog/spdlog.h>

static const uint64_t twoRaisedTo32 = UINT64_C(4294967296);

ASIOError PreparedState::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const {
    if (tStamp) {
        tStamp->lo = _theSystemTime.lo;
        tStamp->hi = _theSystemTime.hi;
    }
    if (sPos) {
        if (_samplePosition >= twoRaisedTo32) {
            sPos->hi = (unsigned long) (_samplePosition / twoRaisedTo32);
            sPos->lo = (unsigned long) (_samplePosition - (sPos->hi * twoRaisedTo32));
        } else {
            sPos->hi = 0;
            sPos->lo = (unsigned long) _samplePosition;
        }
    }
    return ASE_OK;
}

PreparedState::PreparedState(
        const std::vector<IMMDevicePtr> &pDeviceList,
        int sampleRate,
        int bufferSize,
        UserPrefPtr pref,
        ASIOCallbacks *callbacks
) : _callbacks(callbacks), _bufferSize(bufferSize), _sampleRate(sampleRate), _pref(pref),
    _pDeviceList(pDeviceList) {
    _buffers[0].resize(_pref->channelCount);
    _buffers[1].resize(_pref->channelCount);
    for (int i = 0; i < _pref->channelCount; i++) {
        _buffers[0][i].assign(bufferSize, 0);
        _buffers[1][i].assign(bufferSize, 0);
    }
}

PreparedState::~PreparedState() = default;

void PreparedState::InitASIOBufferInfo(ASIOBufferInfo *bufferInfos, int infoCount) {
    for (int i = 0; i < _pref->channelCount; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        info.buffers[0] = _buffers[0].at(info.channelNum).data();
        info.buffers[1] = _buffers[1].at(info.channelNum).data();
    }
}

bool PreparedState::start() {
    mainlog->debug("PreparedState::start");
    if (_runningState) return true; // we are already playing

    // make sure the previous play thread exited
    _samplePosition = 0;
    _bufferIndex = 0;
    try {
        _runningState = std::make_shared<RunningState>(this);
    } catch (AppException &e) {
        mainlog->error("Cannot create runningState: {}", e.what());
        return false;
    }
    return true;
}

bool PreparedState::stop() {
    mainlog->debug("PreparedState::stop");
    _runningState = nullptr;
    return true;
}

void PreparedState::outputReady() {
    if (_runningState) _runningState->signalOutputReady();
}

void PreparedState::requestReset() {
    _callbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
}

static void getNanoSeconds(ASIOTimeStamp *ts) {
    double nanoSeconds = accurateTime() * 1'000'000'000.;
    const double twoRaisedTo32 = 4294967296.;
    ts->hi = (unsigned long) (nanoSeconds / twoRaisedTo32);
    ts->lo = (unsigned long) (nanoSeconds - (ts->hi * twoRaisedTo32));
}

void PreparedState::bufferSwitch(long doubleBufferIndex, ASIOBool directProcess) {
    getNanoSeconds(&_theSystemTime);
    _callbacks->bufferSwitch(doubleBufferIndex, directProcess);
}

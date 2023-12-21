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

PreparedState::PreparedState(const std::vector<IMMDevicePtr> &pDeviceList, ASIOCallbacks *callbacks)
        : _callbacks(callbacks), _bufferSize(_settings.bufferSize), _settings(loadDriverSettings()),
          _pDeviceList(pDeviceList) {
    auto bufferSize = _settings.bufferSize;
    _buffers[0].resize(_settings.channelCount);
    _buffers[1].resize(_settings.channelCount);
    for (int i = 0; i < _settings.channelCount; i++) {
        _buffers[0][i].assign(bufferSize, 0);
        _buffers[1][i].assign(bufferSize, 0);
    }
}

PreparedState::~PreparedState() = default;

void PreparedState::InitASIOBufferInfo(ASIOBufferInfo *bufferInfos, int infoCount) {
    for (int i = 0; i < _settings.channelCount; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        info.buffers[0] = _buffers[0].at(info.channelNum).data();
        info.buffers[1] = _buffers[1].at(info.channelNum).data();
    }
}

bool PreparedState::start() {
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
    _runningState = nullptr;
    return true;
}

void PreparedState::outputReady() {
    if (_runningState) _runningState->signalOutputReady();
}

void PreparedState::requestReset() {
    _callbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
}

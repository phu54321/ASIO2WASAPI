//
// Created by whyask37 on 2023-06-30.
//

#include "PreparedState.h"
#include "RunningState.h"
#include "../utils/WASAPIUtils.h"

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

PreparedState::PreparedState(const std::shared_ptr<IMMDevice> &pDevice, DriverSettings _settings,
                             ASIOCallbacks *callbacks)
        : _settings(std::move(_settings)), _callbacks(callbacks), _bufferSize(_settings.bufferSize),
          _pDevice(pDevice) {
    if (!pDevice) {
        throw AppException("Cannot find target device");
    }

    auto bufferSize = _settings.bufferSize;
    _buffers[0].resize(_settings.nChannels);
    _buffers[1].resize(_settings.nChannels);
    for (int i = 0; i < _settings.nChannels; i++) {
        _buffers[0][i].assign(bufferSize, 0);
        _buffers[1][i].assign(bufferSize, 0);
    }
}

PreparedState::~PreparedState() = default;

void PreparedState::InitASIOBufferInfo(ASIOBufferInfo *bufferInfos, int infoCount) {
    for (int i = 0; i < _settings.nChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        info.buffers[0] = _buffers[0].at(info.channelNum).data();
        info.buffers[1] = _buffers[0].at(info.channelNum).data();
    }
}

bool PreparedState::start() {
    if (_runningState) return true; // we are already playing

    // make sure the previous play thread exited
    _samplePosition = 0;
    _bufferIndex = 0;
    _runningState = std::make_shared<RunningState>(this);
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

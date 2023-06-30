//
// Created by whyask37 on 2023-06-30.
//

#include "PreparedState.h"
#include "RunningState.h"
#include "../utils/WASAPIUtils.h"

static const uint64_t twoRaisedTo32 = UINT64_C(4294967296);

ASIOError PreparedState::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const {
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

PreparedState::PreparedState(const std::shared_ptr<IMMDevice> &pDevice, DriverSettings _settings,
                             ASIOCallbacks *callbacks)
        : settings(std::move(_settings)), m_callbacks(callbacks), m_bufferSize(settings.bufferSize),
          pDevice(pDevice) {
    if (!pDevice) {
        throw AppException("Cannot find target device");
    }

    auto bufferSize = settings.bufferSize;
    m_buffers[0].resize(settings.nChannels);
    m_buffers[1].resize(settings.nChannels);
    for (int i = 0; i < settings.nChannels; i++) {
        m_buffers[0][i].assign(bufferSize, 0);
        m_buffers[1][i].assign(bufferSize, 0);
    }
}

PreparedState::~PreparedState() = default;

void PreparedState::InitASIOBufferInfo(ASIOBufferInfo *bufferInfos, int infoCount) {
    for (int i = 0; i < settings.nChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        info.buffers[0] = m_buffers[0].at(info.channelNum).data();
        info.buffers[1] = m_buffers[0].at(info.channelNum).data();
    }
}

bool PreparedState::start() {
    if (runningState) return true; // we are already playing

    // make sure the previous play thread exited
    m_samplePosition = 0;
    m_bufferIndex = 0;
    runningState = std::make_shared<RunningState>(this);
    return true;
}

bool PreparedState::stop() {
    runningState = nullptr;
    return true;
}

void PreparedState::outputReady() {
    if (runningState) runningState->signalOutputReady();
}

void PreparedState::requestReset() {
    m_callbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
}

//
// Created by whyask37 on 2023-06-30.
//

#ifndef ASIO2WASAPI2_PREPAREDSTATE_H
#define ASIO2WASAPI2_PREPAREDSTATE_H

#include <memory>
#include <vector>
#include "asiosys.h"
#include "asio.h"
#include "../WASAPIOutput/WASAPIOutput.h"
#include "ASIO2WASAPI2Impl.h"

class RunningState;

struct PreparedState {
    DriverSettings settings;
    std::shared_ptr<IMMDevice> pDevice;

    int m_bufferSize = 0; // in audio frames
    int m_bufferIndex = 0;
    std::vector<std::vector<short>> m_buffers[2];
    ASIOCallbacks *m_callbacks = nullptr;

    ASIOTimeStamp m_theSystemTime = {0, 0};
    uint64_t m_samplePosition = 0;
    std::shared_ptr<RunningState> runningState;

    ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const;
};


#endif //ASIO2WASAPI2_PREPAREDSTATE_H

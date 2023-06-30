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

class PreparedState {
    friend class RunningState;

public:
    PreparedState(const std::shared_ptr<IMMDevice> &pDevice, DriverSettings settings, ASIOCallbacks *callbacks);

    ~PreparedState();

    void InitASIOBufferInfo(ASIOBufferInfo *infos, int infoCount);

    bool start();

    bool stop();

public:
    void outputReady();

    void requestReset();

    ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) const;

    int getBufferSize() const { return _bufferSize; }

private:

    DriverSettings _settings;
    ASIOCallbacks *_callbacks;
    std::shared_ptr<IMMDevice> _pDevice;

    int _bufferSize = 0; // in audio frames
    int _bufferIndex = 0;
    std::vector<std::vector<short>> _buffers[2];

    ASIOTimeStamp _theSystemTime = {0, 0};
    uint64_t _samplePosition = 0;
    std::shared_ptr<RunningState> _runningState;

};


#endif //ASIO2WASAPI2_PREPAREDSTATE_H

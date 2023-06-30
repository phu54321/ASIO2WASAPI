//
// Created by whyask37 on 2023-06-30.
//

#ifndef ASIO2WASAPI2_RUNNINGSTATE_H
#define ASIO2WASAPI2_RUNNINGSTATE_H

#include <Windows.h>
#include "PreparedState.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

class RunningState {
public:
    explicit RunningState(PreparedState *p);

    ~RunningState();

    void signalOutputReady();

    void signalPoll();

private:
    void signalStop();

    PreparedState *_preparedState;
    bool _isOutputReady = true;
    bool _shouldPoll = true;
    bool _pollStop = false;

    std::mutex _mutex;
    std::condition_variable _notifier;
    std::thread _pollThread;

    std::unique_ptr<WASAPIOutput> _output;

    static void threadProc(RunningState *state);
};

#endif //ASIO2WASAPI2_RUNNINGSTATE_H

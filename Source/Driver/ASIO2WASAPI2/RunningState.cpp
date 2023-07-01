//
// Created by whyask37 on 2023-06-30.
//

#include "RunningState.h"
#include "../utils/logger.h"
#include <spdlog/spdlog.h>
#include <utility>
#include <cassert>
#include <avrt.h>

#include "../WASAPIOutput/WASAPIOutputEvent.h"

RunningState::RunningState(PreparedState *p)
        : _preparedState(p) {
    SPDLOG_TRACE_FUNC;

    for (int i = 0; i < p->_pDeviceList.size(); i++) {
        auto &device = p->_pDeviceList[i];
        if (i == 0) {
            auto output = std::make_unique<WASAPIOutputEvent>(
                    device,
                    p->_settings.nChannels,
                    p->_settings.nSampleRate,
                    p->_bufferSize,
                    [this]() {
                        signalPoll();
                    });
            _outputList.push_back(std::move(output));
        } else {
            // TODO: add WASAPIOutputPull
            break;
        }
    }

    _pollThread = std::thread(RunningState::threadProc, this);
}

RunningState::~RunningState() {
    SPDLOG_TRACE_FUNC;
    signalStop();
    _pollThread.join();
}

void RunningState::signalOutputReady() {
    SPDLOG_TRACE_FUNC;
    {
        mainlog->trace("[RunningState::signalOutputReady] locking mutex");
        std::lock_guard<std::mutex> lock(_mutex);
        _isOutputReady = true;
        mainlog->trace("[RunningState::signalOutputReady] unlocking mutex");
    }
    _notifier.notify_all();
}

void RunningState::signalStop() {
    SPDLOG_TRACE_FUNC;
    {
        mainlog->trace("[RunningState::signalStop] locking mutex");
        std::lock_guard<std::mutex> lock(_mutex);
        _pollStop = true;
        mainlog->trace("[RunningState::signalStop] unlocking mutex");
    }
    _notifier.notify_all();
}

void RunningState::signalPoll() {
    SPDLOG_TRACE_FUNC;
    {
        mainlog->trace("[RunningState::signalPoll] locking mutex");
        std::lock_guard<std::mutex> lock(_mutex);
        _shouldPoll = true;
        mainlog->trace("[RunningState::signalPoll] unlocking mutex");
    }
    _notifier.notify_all();
}

void RunningState::threadProc(RunningState *state) {
    auto &_preparedState = state->_preparedState;
    auto bufferSize = state->_preparedState->_settings.bufferSize;

    // Ask MMCSS to temporarily boost the runThread priority
    // to reduce the possibility of glitches while we play.
    DWORD taskIndex = 0;
    AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);


    while (true) {
        mainlog->trace("[RunningState::threadProc] Locking mutex");
        std::unique_lock<std::mutex> lock(state->_mutex);
        if (state->_pollStop) break;
        else if (state->_shouldPoll) {
            mainlog->trace("[RunningState::threadProc] _shouldPoll");
            // Wait for output
            if (!state->_isOutputReady) {
                mainlog->trace("[RunningState::threadProc] unlock mutex d/t notifier wait");
                state->_notifier.wait(lock, [state]() {
                    return state->_isOutputReady || state->_pollStop;
                });
                mainlog->trace("[RunningState::threadProc] re-lock mutex after notifier wait");
            }
            if (state->_pollStop) break;
            state->_isOutputReady = false;
            state->_shouldPoll = false;
            mainlog->trace("[RunningState::threadProc] unlock mutex after flag set");
            lock.unlock();

            assert(_preparedState);
            int currentBufferIndex = _preparedState->_bufferIndex;
            const auto &currentBuffer = _preparedState->_buffers[currentBufferIndex];
            mainlog->debug("[RunningState::threadProc] Writing {} samples from buffer {}", bufferSize,
                           currentBufferIndex);
            for (auto &output: state->_outputList) {
                output->pushSamples(currentBuffer);
            }

            mainlog->debug("[RunningState::threadProc] Switching to buffer {}", 1 - currentBufferIndex);
            _preparedState->_callbacks->bufferSwitch(1 - currentBufferIndex, ASIOTrue);
            _preparedState->_bufferIndex = 1 - currentBufferIndex;
        } else {
            mainlog->trace("[RunningState::threadProc] Unlock mutex & waiting");
            state->_notifier.wait(lock, [state]() {
                return state->_pollStop || state->_shouldPoll;
            });
        }
    }
}

//
// Created by whyask37 on 2023-06-30.
//

#include "RunningState.h"
#include "../utils/logger.h"
#include <utility>
#include <cassert>

RunningState::RunningState(PreparedState *p)
        : _preparedState(p) {
    LOGGER_TRACE_FUNC;
    _output = std::make_unique<WASAPIOutput>(
            p->pDevice,
            p->settings.nChannels,
            p->settings.nSampleRate,
            p->m_bufferSize);

    _pollThread = std::thread(RunningState::threadProc, this);
}

RunningState::~RunningState() {
    LOGGER_TRACE_FUNC;
    signalStop();
    _pollThread.join();
}

void RunningState::signalOutputReady() {
    LOGGER_TRACE_FUNC;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _isOutputReady = true;
    }
    _notifier.notify_all();
}

void RunningState::signalStop() {
    LOGGER_TRACE_FUNC;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pollStop = true;
    }
    _notifier.notify_all();
}

void RunningState::threadProc(RunningState *state) {
    auto &_preparedState = state->_preparedState;
    while (true) {
        Logger::trace("Locking mutex");
        std::unique_lock<std::mutex> lock(state->_mutex);
        if (state->_pollStop) break;
        else if (state->_isOutputReady) {
            Logger::trace("Unlock d/t outputReady");
            state->_isOutputReady = false;
            lock.unlock();

            assert(_preparedState);
            int currentBufferIndex = _preparedState->m_bufferIndex;
            const auto &currentBuffer = _preparedState->m_buffers[currentBufferIndex];
            state->_output->pushSamples(currentBuffer);
            _preparedState->m_callbacks->bufferSwitch(1 - currentBufferIndex, ASIOTrue);
            _preparedState->m_bufferIndex = 1 - currentBufferIndex;
        } else {
            Logger::trace("Unlock & waiting");
            state->_notifier.wait(lock, [state]() {
                return state->_pollStop || state->_isOutputReady;
            });
        }
    }
}

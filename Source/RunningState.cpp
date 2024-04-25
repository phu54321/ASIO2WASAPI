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

#include "RunningState.h"
#include "utils/logger.h"
#include "utils/intervalBlock.h"
#include "utils/raiiUtils.h"
#include <spdlog/spdlog.h>
#include <utility>
#include <cassert>
#include <avrt.h>
#include <timeapi.h>
#include <mmsystem.h>
#include <deque>

#include "WASAPIOutput/WASAPIOutputEvent.h"
#include "utils/accurateTime.h"
#include "utils/clampSample.h"
#include <tracy/Tracy.hpp>
#include "res/resource.h"

using namespace std::chrono_literals;

extern HINSTANCE g_hInstDLL;


RunningState::RunningState(PreparedState *p)
        : _preparedState(p),
          _clapSource(p->_sampleRate, p->_pref->clapGain),
          _keyListener(p->_pref->throttle) {
    ZoneScoped;
    std::shared_ptr<WASAPIOutputEvent> mainOutput;

    auto driverSettings = p->_pref;

    for (int i = 0; i < p->_pDeviceList.size(); i++) {
        auto &device = p->_pDeviceList[i];
        auto mode = (i == 0) ? WASAPIMode::Exclusive : WASAPIMode::Shared;
        auto output = std::make_shared<WASAPIOutputEvent>(
                device,
                driverSettings,
                p->_sampleRate,
                p->_bufferSize,
                mode,
                2);

        if (i == 0) {
            _msgWindow.setTrayTooltip(fmt::format(
                    TEXT("Sample rate {}, ASIO input buffer size {} ({:.2f}ms), WASAPI output buffer size {} ({:.2f}ms)"),
                    p->_sampleRate,
                    p->_bufferSize,
                    1000.0 * p->_bufferSize / p->_sampleRate,
                    output->getOutputBufferSize(),
                    1000.0 * output->getOutputBufferSize() / p->_sampleRate));
            _mainOutput = output;
        }
        _outputList.push_back(std::move(output));
    }

    _pollThread = std::thread(RunningState::threadProc, this);
}

RunningState::~RunningState() {
    ZoneScoped;
    signalStop();
    _pollThread.join();
}

void RunningState::signalOutputReady() {
    ZoneScoped;
    mainlog->debug("signalOutputReady");
    {
        std::lock_guard lock(_mutex);
        _isOutputReady = true;
    }
    _notifier.notify_all();
}

void RunningState::signalStop() {
    ZoneScoped;
    {
        std::lock_guard lock(_mutex);
        _pollStop = true;
    }
    _notifier.notify_all();
}

void RunningState::threadProc(RunningState *state) {
    auto &preparedState = state->_preparedState;
    auto bufferSize = preparedState->_bufferSize;
    auto channelCount = preparedState->_pref->channelCount;

    std::vector<std::vector<int32_t>> outputBuffer(preparedState->_pref->channelCount);
    for (auto &buf: outputBuffer) {
        buf.resize(preparedState->_bufferSize);
    }

    // Ask MMCSS to temporarily boost the runThread priority
    // to reduce the possibility of glitches while we play.
    DWORD taskIndex = 0;
    AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);

    TIMECAPS tcaps;
    timeGetDevCaps(&tcaps, sizeof(tcaps));
    timeBeginPeriod(tcaps.wPeriodMin);
    mainlog->debug("timeBeginPeriod({})", tcaps.wPeriodMin);

    IntervalBlock ioLatencyLogBlock(1.0);
    int64_t currentOutputFrame = 0;

    while (true) {
        mainlog->trace("[RunningState::threadProc] Locking mutex");
        std::unique_lock lock(state->_mutex);
        state->_notifier.wait(lock);

        if (ioLatencyLogBlock.due()) {
            auto ioLatency = currentOutputFrame - state->_mainOutput->playedSampleCount();
            mainlog->info("[RunningState::threadProc] IO buffer latency: {} frames", ioLatency);
        }

        // Timer event
        if (state->_pollStop) break;
        else if (state->_isOutputReady) {
            ZoneScopedN("[RunningState::threadProc] _isOutputReady");

            // Put asio main input.
            {
                ZoneScopedN("[RunningState::threadProc] _shouldPoll - Polling ASIO data");
                // Copy data from asio side
                int currentAsioBufferIndex = preparedState->_bufferIndex;
                mainlog->trace("[RunningState::threadProc] Writing {} samples from buffer {}", bufferSize,
                               currentAsioBufferIndex);
                const auto &asioCurrentBuffer = preparedState->_buffers[currentAsioBufferIndex];
                for (size_t ch = 0; ch < channelCount; ch++) {
                    for (size_t i = 0; i < bufferSize; i++) {
                        int32_t sample = asioCurrentBuffer[ch][i];
                        sample >>= 8;  // Scale 32bit to 24bit (To prevent overflow in later steps)`
                        sample -= (sample >> 4);  // multiply 15/16 ( = 0.9375 ) for later compression
                        outputBuffer[ch][i] = sample;
                    }
                }
                mainlog->trace("[RunningState::threadProc] Switching to buffer {}", 1 - currentAsioBufferIndex);
                preparedState->bufferSwitch(1 - currentAsioBufferIndex, ASIOTrue);
                preparedState->_bufferIndex = 1 - currentAsioBufferIndex;
            }

            // Add additional sources
            {
                state->_clapSource.render(currentOutputFrame, &outputBuffer);
            }

            // TODO: add additional processing

            // Rescale & compress output
            for (auto &channel: outputBuffer) {
                for (int &sample: channel) {
                    double normalizedSample = sample / (double) (1 << 23);
                    sample = (int32_t) (0x7fffffff * clampSample(normalizedSample));
                }
            }

            // Output
            {
                ZoneScopedN("[RunningState::threadProc] _shouldPoll - Pushing outputs");
                for (auto &output: state->_outputList) {
                    output->pushSamples(outputBuffer);
                }
            }

            currentOutputFrame += bufferSize;
        }
    }

    timeEndPeriod(tcaps.wPeriodMin);
}

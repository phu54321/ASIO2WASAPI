// Copyright (C) 2024 Hyunwoo Park
//
// This file is part of trgkASIO.
//
// trgkASIO is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// trgkASIO is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

//
// Created by whyask37 on 2024-04-24.
//

#include "KeyboardClapSource.h"
#include "../../utils/accurateTime.h"
#include "../../res/resource.h"
#include "tracy/Tracy.hpp"

extern HINSTANCE g_hInstDLL;


KeyboardClapSource::KeyboardClapSource(int sampleRate, double clapGain)
        : _sampleRate(sampleRate), _clapGain(clapGain),
          _clapRenderer(g_hInstDLL, {
                          MAKEINTRESOURCE(IDR_CLAP_K70_KEYDOWN),
                          MAKEINTRESOURCE(IDR_CLAP_K70_KEYUP)
          }, sampleRate) {

}

void KeyboardClapSource::_pollKeys(int64_t currentFrame) {
    // Update keydown queue for clap sound
    auto pressCount = _keyListener.pollKeyEventCount();
    if (pressCount.keyDown > 0 || pressCount.keyUp) {
        auto &entry = clapQueue[clapQueueIndex];
        int j = 0;

        entry.startFrame = currentFrame;
        for (int i = 0; i < pressCount.keyDown && j < maxConcurrentKeyCount; i++) {
            entry.eventId[j++] = INDEX_KEYDOWN;
        }
        for (int i = 0; i < pressCount.keyUp && j < maxConcurrentKeyCount; i++) {
            entry.eventId[j++] = INDEX_KEYUP;
        }
        entry.eventId[j] = -1;
        clapQueueIndex = (clapQueueIndex + 1) % clapQueueSize;
    }

    // GC old keydown events
    int64_t cutoffFrame = currentFrame - (int64_t) round(_sampleRate * _clapRenderer.getMaxClapSoundLength());
    for (int i = 0; i < clapQueueSize; i++) {
        if (clapQueue[i].eventId[0] >= 0 && clapQueue[i].startFrame < cutoffFrame) {
            clapQueue[i].eventId[0] = -1;
        }
    }
}

void KeyboardClapSource::render(int64_t currentFrame, std::vector<std::vector<int32_t>> *outputBuffer) {
    _pollKeys(currentFrame);

    int channelCount = outputBuffer->size();

    ZoneScopedN("[RunningState::threadProc] _shouldPoll - Add clap sound");
    for (int i = 0; i < clapQueueSize; i++) {
        auto &pair = clapQueue[i];
        if (pair.eventId[0] >= 0) {
            for (size_t ch = 0; ch < channelCount; ch++) {
                for (auto eventId: pair.eventId) {
                    if (eventId == -1) break;
                    _clapRenderer.render(
                            &outputBuffer->at(ch),
                            currentFrame - pair.startFrame,
                            eventId,
                            _clapGain);
                }
            }
        }
    }


    for (int i = 0; i < clapQueueSize; i++) {
        auto &keydownEntry = clapQueue[i];
        if (keydownEntry.eventId[0] >= 0) {
            for (int ch = 0; ch < outputBuffer->size(); ch++) {
                for (auto eventId: keydownEntry.eventId) {
                    if (eventId == -1) break;
                    _clapRenderer.render(
                            &outputBuffer->at(ch),
                            currentFrame,
                            eventId,
                            _clapGain);
                }
            }
        }
    }
}
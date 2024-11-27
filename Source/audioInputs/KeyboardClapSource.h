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

#ifndef TRGKASIO_KEYBOARDCLAPSOURCE_H
#define TRGKASIO_KEYBOARDCLAPSOURCE_H

#include <vector>
#include "../MessageWindow/KeyDownListener.h"
#include "ClapRenderer.h"
#include "./AudioSource.h"

class KeyboardClapSource: public AudioSource {
public:
    KeyboardClapSource(int sampleRate, double clapGain);

    ~KeyboardClapSource() override = default;

    void render(int64_t currentFrame, std::vector<std::vector<int32_t>> *outputBuffer) override;

private:
    static const int clapQueueSize = 256;
    static const int maxConcurrentKeyCount = 16;
    static const int INDEX_KEYDOWN = 0;
    static const int INDEX_KEYUP = 1;

    ClapRenderer _clapRenderer;
    int _sampleRate;
    double _clapGain;

    struct KeyDownPair {
        int64_t startFrame = -1;
        int eventId[maxConcurrentKeyCount + 1];  // +1 so that last element is always -1

        KeyDownPair() {
            std::fill(eventId, eventId + maxConcurrentKeyCount, -1);
        }
    };

    // Fixed-size looping queue
    // This is intentionally designed to overflow after keyDownQueueSize
    // to prevent additional allocation
    std::vector<KeyDownPair> clapQueue{clapQueueSize};
    int clapQueueIndex = 0;

    KeyDownListener _keyListener;

    void _pollKeys(int64_t currentFrame);
};


#endif //TRGKASIO_KEYBOARDCLAPSOURCE_H

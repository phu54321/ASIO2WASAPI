// Copyright (C) 2023 Hyunwoo Park
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
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef TRGKASIO_KEYDOWNLISTENER_H
#define TRGKASIO_KEYDOWNLISTENER_H

#include <thread>
#include <Windows.h>
#include "../utils/SynchronizedClock.h"

struct KeyEventCount {
    int keyDown;
    int keyUp;
};

class KeyDownListener {
public:
    KeyDownListener();

    ~KeyDownListener();

    KeyEventCount pollKeyEventCount();

private:
    static void _tickNotifierCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

    UINT _timerID;
    UINT _wPeriodMin;
    std::mutex _tickMutex;

    bool _keyPressed[256] = {false};
    bool _initialRun = true;

    std::mutex _keyCountMutex;
    int _keyDownCount{0};
    int _keyUpCount{0};
};

#endif //TRGKASIO_KEYDOWNLISTENER_H

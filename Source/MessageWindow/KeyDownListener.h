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
#include <atomic>

struct KeyEventCount {
    int keyDown;
    int keyUp;
};

class KeyDownListener {
public:
    explicit KeyDownListener(bool cpuThrottle = true);

    ~KeyDownListener();

    KeyEventCount pollKeyEventCount();

private:
    static void threadProc(KeyDownListener *p);

    const bool _cpuThrottle;
    std::thread _thread;
    volatile bool _killThread = false;
    std::atomic<int> _keyDownCount{0};
    std::atomic<int> _keyUpCount{0};
};

#endif //TRGKASIO_KEYDOWNLISTENER_H

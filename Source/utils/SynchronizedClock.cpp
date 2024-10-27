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
// Created by whyask37 on 2024-04-26.
//

#include "SynchronizedClock.h"

void SynchronizedClock::tick() {
    {
        std::unique_lock lock(_mutex);
        _counter++;
    }
    _notifier.notify_one();
}

void SynchronizedClock::wait_for(int ms) {
    std::unique_lock lock(_mutex);
    if (_counter > 0) {
        _counter--;
    } else {
        _notifier.wait_for(lock, std::chrono::milliseconds(ms), [this]() { return _counter > 0; });
        _counter--;
    }
}

static void CALLBACK _tickClockCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    auto clock = reinterpret_cast<SynchronizedClock *>(dwUser);
    clock->tick();
}

MMRESULT timeTickClockTimer(UINT wPeriodMin, SynchronizedClock *clock) {
    return timeSetEvent(
            1, wPeriodMin,
            _tickClockCallback,
            reinterpret_cast<DWORD_PTR>(clock),
            TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);

}

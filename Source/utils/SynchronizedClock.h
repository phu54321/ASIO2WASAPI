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

#ifndef TRGKASIO_SYNCHRONIZEDCLOCK_H
#define TRGKASIO_SYNCHRONIZEDCLOCK_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <Windows.h>
#include <mmsystem.h>

class SynchronizedClock {
public:
    SynchronizedClock() = default;

    ~SynchronizedClock() = default;

    void tick();

    void wait_for(int ms);

private:
    int _counter = 0;
    std::mutex _mutex;
    std::condition_variable _notifier;
};

MMRESULT timeTickClockTimer(UINT wPeriodMin, SynchronizedClock *clock);

#endif //TRGKASIO_SYNCHRONIZEDCLOCK_H
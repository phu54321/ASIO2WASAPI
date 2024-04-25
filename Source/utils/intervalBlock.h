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
// Created by whyask37 on 2024-04-25.
//

#ifndef TRGKASIO_INTERVALBLOCK_H
#define TRGKASIO_INTERVALBLOCK_H

#include "accurateTime.h"

class IntervalBlock {
public:
    IntervalBlock(double interval) : _interval(interval), _startTime(accurateTime()) {}

    ~IntervalBlock() = default;

    bool due() {
        double currentTime = accurateTime();
        if (currentTime - _startTime >= _interval) {
            _startTime += _interval;
            return true;
        }
        return false;
    }

private:
    double _interval, _startTime;
};

#endif //TRGKASIO_INTERVALBLOCK_H

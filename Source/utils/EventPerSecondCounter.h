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
// Created by whyask37 on 2024-10-27.
//

#ifndef TRGKASIO_EVENTPERSECONDCOUNTER_H
#define TRGKASIO_EVENTPERSECONDCOUNTER_H

#include "accurateTime.h"
#include <utility>
#include <vector>
#include <string>
#include "logger.h"

class EventPerSecondCounter {
public:
    const int logMinimumCount = 10;

    EventPerSecondCounter(std::wstring label, double logInterval = 1.0) : _label(std::move(label)),
                                                                          _logInterval(logInterval) {
        _lastTickTime = _tickStart = accurateTime();
    }

    ~EventPerSecondCounter() = default;

    void tick() {
        double now = accurateTime();
        _tickDurations.push_back(now - _lastTickTime);
        _lastTickTime = now;

        if (_tickDurations.size() >= logMinimumCount && now - _tickStart >= _logInterval) {
            double avgTickDuration = (now - _tickStart) / _tickDurations.size();
            auto last1PercentSize = _tickDurations.size() / 100.0;
            auto last1PercentSizeInt = int(last1PercentSize);
            double last1PercentSizeFractional = last1PercentSize - last1PercentSizeInt;
            std::nth_element(_tickDurations.begin(), _tickDurations.begin() + last1PercentSizeInt, _tickDurations.end(),
                             std::greater{});
            auto last1PercentDuration =
                    *(_tickDurations.begin() + last1PercentSizeInt) * (1 - last1PercentSizeFractional) +
                    *(_tickDurations.begin() + last1PercentSizeInt + 1);
            mainlog->info(L"{} avg {:.1f}PS, low1% {:.1f}PS", _label, 1 / avgTickDuration, 1 / last1PercentDuration);

            _tickStart = now;
        }
    }

private:
    std::wstring _label;
    std::vector<double> _tickDurations;
    double _tickStart, _lastTickTime;
    double _logInterval;
};

#endif //TRGKASIO_EVENTPERSECONDCOUNTER_H

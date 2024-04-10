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
// Created by whyask37 on 2024-04-10.
//

#include "clampSample.h"
#include <tracy/Tracy.hpp>

double clampSample(double sample) {
    const float compressionPadding = 0.0625;  // 1 / 16
    const float compressionThresholdHigh = 1 - compressionPadding;
    const float compressionThresholdLow = -compressionThresholdHigh;

    if (sample > compressionThresholdHigh) {
        auto overflow = sample - compressionThresholdHigh;
        return compressionThresholdHigh + compressionPadding * (2 / (1 + expf(-overflow / compressionPadding)) - 1);
    } else if (sample < compressionThresholdLow) {
        auto overflow = sample - compressionThresholdLow;
        return compressionThresholdLow + compressionPadding * (2 / (1 + expf(-overflow / compressionPadding)) - 1);
    } else {
        return sample;
    }
}


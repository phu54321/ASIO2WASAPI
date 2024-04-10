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

#ifndef TRGKASIO_CLAMPSAMPLE_H
#define TRGKASIO_CLAMPSAMPLE_H

#include <vector>

#
/**
 * Clamp sample to [-1, 1] range. Useful when adding multiple audio samples.
 *
 * @param sample Input sample.
 * @returns Sample clamped to [-1, 1] range.
 */
double clampSample(double sample);

#endif //TRGKASIO_CLAMPSAMPLE_H

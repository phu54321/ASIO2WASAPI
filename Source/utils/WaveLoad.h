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

//
// Created by whyask37 on 2023-07-14.
//

#ifndef TRGKASIO_WAVELOAD_H
#define TRGKASIO_WAVELOAD_H

#include <vector>

struct WaveSound {
    int sampleRate = 0;
    std::vector<double> audio;
};

WaveSound loadWaveSound(const std::vector<BYTE> &content, int targetSampleRate);


#endif //TRGKASIO_WAVELOAD_H

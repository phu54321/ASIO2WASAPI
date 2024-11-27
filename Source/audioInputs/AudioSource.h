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
// Created by whyask37 on 2024-10-31.
//

#ifndef TRGKASIO_AUDIOSOURCE_H
#define TRGKASIO_AUDIOSOURCE_H

#include <cstdint>
#include <vector>

class AudioSource {
public:
    AudioSource() = default;

    virtual ~AudioSource() = default;

    virtual void render(int64_t currentFrame, std::vector <std::vector<int32_t>> *outputBuffer) = 0;
};

#endif //TRGKASIO_AUDIOSOURCE_H

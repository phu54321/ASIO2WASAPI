// Copyright (C) 2023 Hyunwoo Park
//
// This file is part of ASIO2WASAPI2.
//
// ASIO2WASAPI2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ASIO2WASAPI2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

//
// Created by whyask37 on 2023-07-11.
//

#ifndef ASIO2WASAPI2_CLAPRENDERER_H
#define ASIO2WASAPI2_CLAPRENDERER_H

#include <Windows.h>
#include <vector>

class ClapRenderer {
public:
    ClapRenderer(HMODULE hDLL, double gain, int targetSampleRate);

    ~ClapRenderer() = default;

    double getClapSoundLength() const;

    void render(std::vector<int32_t> *output, double renderTime, double clapStartTime, int gain) const;

private:
    std::vector<int32_t> _samples;
    int _sampleRate;
};

#endif //ASIO2WASAPI2_CLAPRENDERER_H

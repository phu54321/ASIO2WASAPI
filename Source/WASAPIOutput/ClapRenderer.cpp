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

#include "ClapRenderer.h"
#include "../utils/ResourceLoad.h"
#include "../res/resource.h"
#include "../utils/logger.h"
#include "../utils/WaveLoad.h"
#include "../lib/r8brain_free_src/CDSPResampler.h"
#include <tracy/Tracy.hpp>

#include <vector>
#include <mmsystem.h>

/////// IDK the use case, but user MAY replace clap sound resource
// with their own things...

WaveSound loadWaveResource(HMODULE hDLL, int targetSampleRate, const TCHAR *resName) {
    auto clapSoundWAV = loadUserdataResource(hDLL, resName);
    return loadWaveSound(clapSoundWAV, targetSampleRate);
}

ClapRenderer::ClapRenderer(HMODULE hDLL, const std::vector<LPCTSTR> &resList, int targetSampleRate) {
    try {
        double maxTime = 0;
        for (auto resName: resList) {
            auto res = loadWaveResource(hDLL, targetSampleRate, resName);
            _clapSoundList.push_back(res);
            double time = (double) res.audio.size() / (double) res.sampleRate;
            if (time > maxTime) maxTime = time;
        }
        _maxClapSoundLength = maxTime;
    } catch (AppException &e) {
        mainlog->error("Cannot load clap sound: {}", e.what());
        _maxClapSoundLength = 0;
        _clapSoundList.clear();
    }
}

double ClapRenderer::getMaxClapSoundLength() const {
    return _maxClapSoundLength;
}

void ClapRenderer::render(std::vector<int32_t> *output, int startFrame, int index, double gain) const {
    assert(output);

    ZoneScoped;

    if (_clapSoundList.empty()) return;

    if (index < 0 || index > _clapSoundList.size()) {
        mainlog->warn("ClapRender::render called with OOB index {} (range: 0 ~ {})", index, _clapSoundList.size());
        return;
    }

    auto &clapSound = _clapSoundList[index];

    const auto &samples = clapSound.audio;
    int32_t *outP = output->data();
    for (int i = std::max(0, -startFrame); i < output->size(); i++) {
        auto inPos = i + startFrame;
        if (inPos < 0) continue;
        else if (inPos >= samples.size()) break;
        outP[i] += (int32_t) round(samples[inPos] * gain * (1 << 23));
    }
}

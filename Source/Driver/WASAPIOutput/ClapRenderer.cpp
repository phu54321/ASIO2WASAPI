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

#include "ClapRenderer.h"
#include "../utils/ResourceLoad.h"
#include "../res/resource.h"
#include "../utils/logger.h"
#include "../utils/WaveLoad.h"
#include "../lib/r8brain_free_src/CDSPResampler.h"

#include <vector>
#include <mmsystem.h>

/////// IDK the use case, but user MAY replace clap sound resource
// with their own things...

WaveSound loadWaveResource(HMODULE hDLL, int targetSampleRate, const TCHAR *resName) {
    auto clapSoundWAV = loadUserdataResource(hDLL, resName);
    return loadWaveSound(clapSoundWAV, targetSampleRate);
}

const TCHAR *cherryResNames[] = {
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_01),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_02),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_03),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_04),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_05),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_06),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_07),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_08),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_09),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_10),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_11),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_12),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_13),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_14),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_15),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_16),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_17),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_18),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_19),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_20),
                MAKEINTRESOURCE(IDR_CLAP_CHERRY_21),
};

ClapRenderer::ClapRenderer(HMODULE hDLL, int targetSampleRate) {
    try {
        double maxTime = 0;
        for (auto resName: cherryResNames) {
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

void ClapRenderer::render(std::vector<int32_t> *output, double renderTime, double clapStartTime, int rng,
                          double gain) const {
    assert(output);

    if (_clapSoundList.empty()) return;

    // Pick one
    auto &clapSound = _clapSoundList[rng % _clapSoundList.size()];
    double clapRelTime = (renderTime - clapStartTime);
    int clapStartSamples = (int) round(clapRelTime * clapSound.sampleRate);
    mainlog->trace("clapRelTime {}, clapStartSamples {}", clapRelTime, clapStartSamples);

    // Clap hadn't started
    const auto &samples = clapSound.audio;
    int32_t *outP = output->data();
    for (int i = max(0, -clapStartSamples); i < output->size(); i++) {
        auto inPos = i + clapStartSamples;
        if (inPos < 0) continue;
        else if (inPos >= samples.size()) break;
        outP[i] += (int32_t) round(samples[inPos] * gain * (1 << 24));
    }
}

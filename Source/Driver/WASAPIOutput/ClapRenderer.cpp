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

#include "ClapRenderer.h"
#include "../utils/ResourceLoad.h"
#include "../resource.h"
#include "../utils/logger.h"

#include "../r8brain_free_src/CDSPResampler.h"

#include <vector>
#include <mmsystem.h>

/////// IDK the use case, but user MAY replace clap sound resource
// with their own things...

static BYTE *findChunk(
        BYTE *pStart,
        const BYTE *pEnd,
        const char *headerName,
        size_t *size) {
    BYTE *p = pStart;
    while (true) {
        runtime_check(p + 8 <= pEnd, "Cannot find '{}' section", headerName);
        auto chunkSize = *reinterpret_cast<uint32_t *>(p + 4);
        if (memcmp(p, headerName, 4) == 0) {
            if (size) *size = chunkSize;
            return p + 8;
        }
        runtime_check(chunkSize <= 0x7fffffff, "Section size too big: {}", chunkSize);
        p += 8 + chunkSize;
    }
}


ClapRenderer::ClapRenderer(HMODULE hDLL, double gain, int targetSampleRate) {
    try {
        auto clapSoundWAV = loadUserdataResource(hDLL, MAKEINTRESOURCE(IDR_CLAP_MONO));
        const auto resourceSize = clapSoundWAV.size();
        const BYTE *riffEnd = clapSoundWAV.data() + resourceSize;
        runtime_check(resourceSize > 12, "Not a valid RIFF file");
        runtime_check(memcmp(clapSoundWAV.data(), "RIFF", 4) == 0, "Not a valid RIFF file");
        runtime_check(memcmp(clapSoundWAV.data() + 8, "WAVE", 4) == 0, "Not a valid WAVE file");

        auto riffStart = clapSoundWAV.data() + 12;

        // find "fmt " tag
        size_t fmtSize;
        auto fmtP = findChunk(riffStart, riffEnd, "fmt ", &fmtSize);
        runtime_check(fmtSize >= sizeof(WAVEFORMAT), "Invalid fmt section");
        WAVEFORMAT format;
        memcpy(&format, fmtP, sizeof(WAVEFORMAT));
        runtime_check(format.nChannels == 1 && format.nBlockAlign == 2, "Only mono & 16bit clap sound supported");

        //find "data" tag
        size_t dataSize;
        auto dataP = findChunk(riffStart, riffEnd, "data", &dataSize);
        auto dataShortP = reinterpret_cast<int16_t *>(dataP);

        // Fill in data
        int sampleN = dataSize / format.nBlockAlign;
        std::vector<double> samples(sampleN);
        for (size_t i = 0; i < sampleN; i++) {
            samples[i] = (dataShortP[i] / 32768.) * gain;
        }
        mainlog->debug("clap sampleN {}, sampleRate {}", sampleN, format.nSamplesPerSec);

        // Resample to target
        const size_t resampleChunkSize = 1024;
        auto resampler = std::make_unique<r8b::CDSPResampler24>(
                format.nSamplesPerSec,
                targetSampleRate,
                sampleN);
        double *outputSamples;
        auto outputLength = resampler->process(samples.data(), sampleN, outputSamples);
        _samples.resize(outputLength);
        for (size_t i = 0; i < outputLength; i++) {
            double sample = outputSamples[i];
            if (sample > 1) sample = 1;
            else if (sample < -1) sample = -1;
            _samples[i] = (int32_t) (sample * 0x7fffff);  // 24bit
        }
        mainlog->debug("resample: {} Hz {} samples -> {} Hz {} samples",
                       format.nSamplesPerSec, sampleN,
                       targetSampleRate, outputLength);
        _sampleRate = targetSampleRate;
    } catch (AppException &e) {
        mainlog->error("Cannot load clap sound: {}", e.what());
        _samples.clear();
        _sampleRate = targetSampleRate;
    }
}

double ClapRenderer::getClapSoundLength() const {
    return (double) _samples.size() / (double) _sampleRate;
}

void ClapRenderer::render(std::vector<int32_t> *output, double renderTime, double clapStartTime, int gain) const {
    assert(output);

    double clapRelTime = (renderTime - clapStartTime);
    int clapStartSamples = (int) round(clapRelTime * _sampleRate);
    mainlog->trace("clapRelTime {}, clapStartSamples {}", clapRelTime, clapStartSamples);

    // Clap hadn't started
    int32_t *outP = output->data();
    for (int i = 0; i < output->size(); i++) {
        auto inPos = i + clapStartSamples;
        if (inPos < 0) continue;
        else if (inPos >= _samples.size()) break;
        outP[i] += _samples[inPos] * gain;
    }
}

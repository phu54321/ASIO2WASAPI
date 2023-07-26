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

#include "ResourceLoad.h"
#include "../res/resource.h"
#include "logger.h"
#include "../lib/r8brain_free_src/CDSPResampler.h"
#include "WaveLoad.h"

#include <vector>
#include <mmsystem.h>

/////// IDK the use case, but user MAY replace clap sound resource
// with their own things...

static const BYTE *findChunk(
        const BYTE *pStart,
        const BYTE *pEnd,
        const char *headerName,
        size_t *size) {
    const BYTE *p = pStart;
    while (true) {
        runtime_check(p + 8 <= pEnd, "Cannot find '{}' section", headerName);
        auto chunkSize = *reinterpret_cast<const uint32_t *>(p + 4);
        if (memcmp(p, headerName, 4) == 0) {
            if (size) *size = chunkSize;
            return p + 8;
        }
        runtime_check(chunkSize <= 0x7fffffff, "Section size too big: {}", chunkSize);
        p += 8 + chunkSize;
    }
}

WaveSound loadWaveSound(const std::vector<BYTE> &content, int targetSampleRate) {
    const auto resourceSize = content.size();
    const BYTE *riffEnd = content.data() + resourceSize;
    runtime_check(resourceSize > 12, "Not a valid RIFF file");
    runtime_check(memcmp(content.data(), "RIFF", 4) == 0, "Not a valid RIFF file");
    runtime_check(memcmp(content.data() + 8, "WAVE", 4) == 0, "Not a valid WAVE file");

    auto riffStart = content.data() + 12;

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
    auto dataShortP = reinterpret_cast<const int16_t *>(dataP);

    // Fill in data
    int sampleN = dataSize / format.nBlockAlign;
    std::vector<double> samples(sampleN);
    for (size_t i = 0; i < sampleN; i++) {
        samples[i] = dataShortP[i] / 32768.;
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

    WaveSound output;
    output.sampleRate = targetSampleRate;
    auto &resampledOutput = output.audio;
    resampledOutput.resize(outputLength);
    for (size_t i = 0; i < outputLength; i++) {
        double sample = outputSamples[i];
        if (sample > 1) sample = 1;
        else if (sample < -1) sample = -1;
        resampledOutput[i] = sample;
    }
    mainlog->debug("resample: {} Hz {} samples -> {} Hz {} samples",
                   format.nSamplesPerSec, sampleN,
                   targetSampleRate, outputLength);

    return output;
}


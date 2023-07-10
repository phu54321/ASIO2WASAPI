// Copyright (C) 2023 Hyun Woo Park
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


#pragma once

#ifndef ASIO2WASAPI2_WASAPIOUTPUT_H
#define ASIO2WASAPI2_WASAPIOUTPUT_H

#include <vector>
#include <memory>

class WASAPIOutput {
public:
    virtual ~WASAPIOutput() = default;

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    virtual void pushSamples(const std::vector<std::vector<int32_t>> &buffer) = 0;
};

using WASAPIOutputPtr = std::shared_ptr<WASAPIOutput>;

#endif //ASIO2WASAPI2_WASAPIOUTPUT_H

//
// Created by whyask37 on 2023-06-26.
//

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
    virtual void pushSamples(const std::vector<std::vector<short>> &buffer) = 0;
};

using WASAPIOutputPtr = std::shared_ptr<WASAPIOutput>;

#endif //ASIO2WASAPI2_WASAPIOUTPUT_H

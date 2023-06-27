//
// Created by whyask37 on 2023-06-26.
//

#pragma once

#ifndef ASIO2WASAPI2_WASAPIOUTPUT_H
#define ASIO2WASAPI2_WASAPIOUTPUT_H

#include <mmdeviceapi.h>
#include <string>
#include <memory>
#include <utility>
#include <functional>
#include <vector>

class WASAPIOutputImpl;

class WASAPIOutput {
public:
    WASAPIOutput(const std::shared_ptr<IMMDevice> &pDevice, int nChannels, int sampleRate, int bufferSizeRequest);

    ~WASAPIOutput();

    /**
     * Push samples to ring central queue. This will be printed to asio.
     * @param buffer `sample = buffer[channel][sampleIndex]`
     */
    void pushSamples(const std::vector<std::vector<short>> &buffer);

    void registerCallback(std::function<void()> pullCallback);

private:
    std::unique_ptr<WASAPIOutputImpl> _pImpl;
};

class WASAPIOutputException : public std::exception {
private:
    std::string message;

public:
    explicit WASAPIOutputException(std::string s) : message(std::move(s)) {}

    const char *what() const override {
        return message.c_str();
    }
};

#endif //ASIO2WASAPI2_WASAPIOUTPUT_H

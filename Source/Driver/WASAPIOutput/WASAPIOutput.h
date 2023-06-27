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

class WASAPIOutputImpl;

class WASAPIOutput {
public:
    WASAPIOutput(std::shared_ptr<IMMDevice> pDevice, int nChannels, int sampleRate);

    ~WASAPIOutput();

    /// Push audio data to wasapi render path
    /// \param buffer channel-interleaved 32bit integer samples
    /// \param sampleN number of samples to put in
    void pushSamples(short *buffer, int sampleN);

private:
    std::unique_ptr<WASAPIOutputImpl> _pimpl;
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

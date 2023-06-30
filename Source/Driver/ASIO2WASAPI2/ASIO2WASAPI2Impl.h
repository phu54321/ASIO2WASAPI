//
// Created by whyask37 on 2023-06-29.
//

#pragma once

#ifndef ASIO2WASAPI2_ASIO2WASAPI2IMPL_H
#define ASIO2WASAPI2_ASIO2WASAPI2IMPL_H

#include <Windows.h>
#include <thread>
#include <atomic>
#include "../utils/AppException.h"
#include "../WASAPIOutput/WASAPIOutput.h"
#include "DriverSettings.h"

#include "asiosys.h"
#include "asio.h"
#include "../utils/WASAPIUtils.h"


struct PreparedState;

class RunningState;

class ASIO2WASAPI2Impl {
public:
    explicit ASIO2WASAPI2Impl(void *sysRef);

    ~ASIO2WASAPI2Impl();

    ASIOError getChannels(long *numInputChannels, long *numOutputChannels);

    ASIOError getLatencies(long *inputLatency, long *outputLatency);

    ASIOError canSampleRate(ASIOSampleRate sampleRate);

    ASIOError getSampleRate(ASIOSampleRate *sampleRate);

    ASIOError setSampleRate(ASIOSampleRate sampleRate);

    ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp);

    ASIOError getChannelInfo(ASIOChannelInfo *info);

    ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
                            long bufferSize, ASIOCallbacks *callbacks);

    ASIOError start();

    ASIOError stop();

    ASIOError disposeBuffers();

    ASIOError outputReady();

private:
    DriverSettings _settings{};
    std::vector<IMMDevicePtr> _pDeviceList;
    std::shared_ptr<PreparedState> _preparedState;
};

#endif //ASIO2WASAPI2_ASIO2WASAPI2IMPL_H

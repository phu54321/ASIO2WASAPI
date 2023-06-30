//
// Created by whyask37 on 2023-06-29.
//

#pragma once

#ifndef ASIO2WASAPI2_ASIO2WASAPI2IMPL_H
#define ASIO2WASAPI2_ASIO2WASAPI2IMPL_H

#include <Windows.h>
#include "../utils/AppException.h"
#include "../WASAPIOutput/WASAPIOutput.h"
#include "../utils/TrayOpener.hpp"

#include "asiosys.h"
#include "asio.h"


struct DriverSettings {
    int nChannels = 2;
    int nSampleRate = 48000;
    std::wstring deviceId;
};


class ASIO2WASAPI2Impl {
public:
    ASIO2WASAPI2Impl(void *sysRef);

    ~ASIO2WASAPI2Impl();

    ASIOError getChannels(long *numInputChannels, long *numOutputChannels);

    ASIOError getLatencies(long *inputLatency, long *outputLatency);

    ASIOError getBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity);

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
    void settingsReadFromRegistry();

    void settingsWriteToRegistry();

    // fields valid before initialization
    HWND m_hAppWindowHandle;
    DriverSettings m_settings{};

    // fields filled by init()/cleaned by shutdown()
    std::shared_ptr<IMMDevice> m_pDevice;

    // fields filled by createBuffers()/cleaned by disposeBuffers()
    // ASIO buffers *& callbacks
    int m_bufferSize = 0; // in audio frames
    std::vector<std::vector<short>> m_buffers[2];
    ASIOCallbacks *m_callbacks = nullptr;

    std::unique_ptr<WASAPIOutput> m_output;
    ASIOTimeStamp m_theSystemTime = {0, 0};
    uint64_t m_samplePosition = 0;

    std::unique_ptr<TrayOpener> openerPtr{};
};

#endif //ASIO2WASAPI2_ASIO2WASAPI2IMPL_H

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

#pragma once

#include <vector>
#include <memory>
#include <Windows.h>

#ifndef _INC_MMREG

#include <mmreg.h> // needed for WAVEFORMATEXTENSIBLE

#endif
struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

#include "dllentry/COMBaseClasses.h"
#include "asiosys.h"
#include "iasiodrv.h"

extern const CLSID CLSID_ASIO2WASAPI2_DRIVER;
const char *const szDescription = "ASIO2WASAPI2";

class ASIO2WASAPI2Impl;

class ASIO2WASAPI2 : public IASIO, public CUnknown {
public:
    ASIO2WASAPI2(LPUNKNOWN pUnk, HRESULT *phr);

    ~ASIO2WASAPI2();

    // CUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        return GetOwner()->QueryInterface(riid, ppv);
    };

    STDMETHODIMP_(ULONG)
    AddRef() override { return GetOwner()->AddRef(); };

    STDMETHODIMP_(ULONG)
    Release() override {
        return GetOwner()->Release();
    };

    static CUnknown *CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);

    virtual HRESULT STDMETHODCALLTYPE NonDelegatingQueryInterface(REFIID riid, void **ppvObject) override;

    // IASIO
    ASIOBool init(void *sysRef) override;

    void getDriverName(char *name) override; // max 32 bytes incl. terminating zero
    long getDriverVersion() override;

    void getErrorMessage(char *string) override; // max 128 bytes incl. terminating zero

    ASIOError start() override;

    ASIOError stop() override;

    ASIOError getChannels(long *numInputChannels, long *numOutputChannels) override;

    ASIOError getLatencies(long *inputLatency, long *outputLatency) override;

    ASIOError getBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity) override;

    ASIOError canSampleRate(ASIOSampleRate sampleRate) override;

    ASIOError getSampleRate(ASIOSampleRate *sampleRate) override;

    ASIOError setSampleRate(ASIOSampleRate sampleRate) override;

    ASIOError getClockSources(ASIOClockSource *clocks, long *numSources) override;

    ASIOError setClockSource(long index) override;

    ASIOError getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) override;

    ASIOError getChannelInfo(ASIOChannelInfo *info) override;

    ASIOError createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
                            long bufferSize, ASIOCallbacks *callbacks) override;

    ASIOError disposeBuffers() override;

    ASIOError controlPanel() override;

    ASIOError future(long selector, void *opt) override;

    ASIOError outputReady() override;

private:
    std::unique_ptr<ASIO2WASAPI2Impl> _pImpl;
};

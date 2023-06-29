/*  ASIO2WASAPI2 Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of ASIO2WASAPI2.

    ASIO2WASAPI2 is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIO2WASAPI2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIO2WASAPI2; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <cstdio>
#include <cstring>
#include <cmath>

#include <Windows.h>
#include <timeapi.h>
#include "ASIO2WASAPI2.h"
#include "../resource.h"
#include "../utils/logger.h"
#include "../utils/json.hpp"
#include "../WASAPIOutput/createIAudioClient.h"
#include "../utils/WASAPIUtils.h"
#include "../utils/utf8convert.h"
#include "../utils/raiiUtils.h"

using json = nlohmann::json;

CLSID CLSID_ASIO2WASAPI2_DRIVER = {0xe3226090, 0x473d, 0x4cc9, {0x83, 0x60, 0xe1, 0x23, 0xeb, 0x9e, 0xf8, 0x47}};

const double twoRaisedTo32 = 4294967296.;
const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;
const ASIOSampleType sampleType = ASIOSTInt16LSB;

void ASIO2WASAPI2::clearState() {
    LOGGER_TRACE_FUNC;

    // For safety
    if (m_output) {
        Logger::error(L"m_output still not nullptr on clearState");
        m_output = nullptr;
    }

    // fields valid before initialization
    auto &settings = m_settings;
    settings.nChannels = 2;
    settings.nSampleRate = 48000;
    settings.deviceId.clear();

    // fields filled by init()/cleaned by shutdown()
    m_initialized = false;
    m_pDevice = nullptr;
    m_bufferIndex = 0;
    m_hAppWindowHandle = nullptr;

    // fields filled by createBuffers()/cleaned by disposeBuffers()
    m_buffers[0].clear();
    m_buffers[1].clear();
    m_callbacks = nullptr;

    // fields filled by start()/cleaned by stop()
    m_bufferSize = 0;
    m_theSystemTime.hi = 0;
    m_theSystemTime.lo = 0;
    m_samplePosition = 0;
}

extern HINSTANCE g_hinstDLL;

ASIO2WASAPI2::ASIO2WASAPI2(LPUNKNOWN pUnk, HRESULT *phr)
        : CUnknown(TEXT("ASIO2WASAPI2"), pUnk, phr) {
    clearState();
    settingsReadFromRegistry();

    openerPtr = std::make_unique<TrayOpener>(
            g_hinstDLL,
            LoadIcon(g_hinstDLL, MAKEINTRESOURCE(IDI_ICON1)),
            [&]() { controlPanel(); },
            TEXT("ASIO2WASAPI2: Open Configuration"));
}

ASIO2WASAPI2::~ASIO2WASAPI2() {
    shutdown();
}

void ASIO2WASAPI2::shutdown() {
    stop();
    disposeBuffers();
    clearState();
}

ASIOError ASIO2WASAPI2::controlPanel() {
    LOGGER_TRACE_FUNC;

    extern HINSTANCE g_hinstDLL;
    DialogBoxParam(g_hinstDLL, MAKEINTRESOURCE(IDD_CONTROL_PANEL), m_hAppWindowHandle, (DLGPROC) ControlPanelProc,
                   (LPARAM) this);
    return ASE_OK;
}

BOOL CALLBACK ASIO2WASAPI2::ControlPanelProc(HWND hwndDlg,
                                             UINT message, WPARAM wParam, LPARAM lParam) {
    static ASIO2WASAPI2 *pDriver = nullptr;
    static std::vector<std::wstring> deviceStringIds;
    switch (message) {
        case WM_DESTROY:
            pDriver = nullptr;
            deviceStringIds.clear();
            return 0;
        case WM_COMMAND: {
            LOGGER_TRACE_FUNC;
            switch (LOWORD(wParam)) {
                case IDOK:
                    if (pDriver) {
                        int nChannels = 2;
                        int nSampleRate = 48000;
                        // get nChannels and nSampleRate from the dialog
                        {
                            BOOL bSuccess = FALSE;
                            int tmp = (int) GetDlgItemInt(hwndDlg, IDC_CHANNELS, &bSuccess, TRUE);
                            if (bSuccess && tmp >= 0)
                                nChannels = tmp;
                            else {
                                MessageBox(hwndDlg, TEXT("Invalid number of channels"), szDescription, MB_OK);
                                return 0;
                            }
                            tmp = (int) GetDlgItemInt(hwndDlg, IDC_SAMPLE_RATE, &bSuccess, TRUE);
                            if (bSuccess && tmp >= 0)
                                nSampleRate = tmp;
                            else {
                                MessageBox(hwndDlg, TEXT("Invalid sample rate"), szDescription, MB_OK);
                                return 0;
                            }
                        }
                        // get the selected device's index from the dialog
                        LRESULT lr = SendDlgItemMessage(hwndDlg, IDL_DEVICE, LB_GETCURSEL, 0, 0);
                        if (lr == CB_ERR || lr < 0 || (size_t) lr >= deviceStringIds.size()) {
                            MessageBox(hwndDlg, TEXT("No audio device selected"), szDescription, MB_OK);
                            return 0;
                        }
                        const auto &selectedDeviceId = deviceStringIds[lr];
                        // find this device
                        std::shared_ptr<IMMDevice> pDevice = nullptr;
                        {

                            iterateAudioEndPoints([&](std::shared_ptr<IMMDevice> pMMDevice) {
                                auto deviceId = getDeviceId(pMMDevice);
                                if (deviceId.size() == 0)
                                    return true;
                                if (deviceId == selectedDeviceId) {
                                    pDevice = pMMDevice;
                                    return false;
                                }
                                return true;
                            });
                        }
                        if (!pDevice) {
                            MessageBox(hwndDlg, TEXT("Invalid audio device"), szDescription, MB_OK);
                            return 0;
                        }

                        // make sure the reset request is issued no matter how we proceed
                        class CCallbackCaller {
                            ASIOCallbacks *m_pCallbacks;

                        public:
                            CCallbackCaller(ASIOCallbacks *pCallbacks) : m_pCallbacks(pCallbacks) {}

                            ~CCallbackCaller() {
                                if (m_pCallbacks)
                                    m_pCallbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
                            }
                        } caller(pDriver->m_callbacks);

                        // shut down the driver so no exclusive WASAPI connection would stand in our way
                        HWND hAppWindowHandle = pDriver->m_hAppWindowHandle;
                        pDriver->shutdown();

                        // make sure the device supports this combination of nChannels and nSampleRate
                        BOOL rc = FindStreamFormat(pDevice, nChannels, nSampleRate);
                        if (!rc) {
                            MessageBox(hwndDlg, TEXT("Sample rate is not supported in WASAPI exclusive mode"),
                                       szDescription, MB_OK);
                            return 0;
                        }

                        // copy selected device/sample rate/channel combination into the driver
                        auto &settings = pDriver->m_settings;
                        settings.nChannels = nChannels;
                        settings.nSampleRate = nSampleRate;
                        settings.deviceId = selectedDeviceId;

                        // try to init the driver
                        if (pDriver->init(hAppWindowHandle) == ASIOFalse) {
                            MessageBox(hwndDlg, TEXT("ASIO driver failed to initialize"), szDescription, MB_OK);
                            return 0;
                        }
                        pDriver->settingsWriteToRegistry();
                    }
                    EndDialog(hwndDlg, wParam);
                    return 0;
                case IDCANCEL:
                    EndDialog(hwndDlg, wParam);
                    return 0;
            }
        }
            break;
        case WM_INITDIALOG: {
            const auto &settings = pDriver->m_settings;
            LOGGER_TRACE_FUNC;
            pDriver = (ASIO2WASAPI2 *) lParam;
            if (!pDriver)
                return FALSE;
            SetDlgItemInt(hwndDlg, IDC_CHANNELS, (UINT) settings.nChannels, TRUE);
            SetDlgItemInt(hwndDlg, IDC_SAMPLE_RATE, (UINT) settings.nSampleRate, TRUE);

            CoInitialize(nullptr);

            std::vector<std::wstring> deviceIds;
            if (!iterateAudioEndPoints([&](auto pMMDevice) {
                auto deviceId = getDeviceId(pMMDevice);
                if (deviceId.size() == 0)
                    return false;
                deviceIds.push_back(deviceId);

                auto friendlyName = getDeviceFriendlyName(pMMDevice);
                HRESULT lr;
                if (friendlyName.empty() ||
                    (lr = SendDlgItemMessageW(hwndDlg, IDL_DEVICE, LB_ADDSTRING, -1, (LPARAM) friendlyName.c_str())) ==
                    CB_ERR) {
                    return false;
                }
                return true;
            })) {
                return false;
            }

            deviceStringIds = deviceIds;

            // find current device id
            unsigned nDeviceIdIndex = -1;
            if (!settings.deviceId.empty())
                for (unsigned i = 0; i < deviceStringIds.size(); i++) {
                    if (deviceStringIds[i] == settings.deviceId) {
                        nDeviceIdIndex = i;
                        break;
                    }
                }
            SendDlgItemMessage(hwndDlg, IDL_DEVICE, LB_SETCURSEL, nDeviceIdIndex, 0);
            return TRUE;
        }
            break;
    }
    return FALSE;
}

/*  ASIO driver interface implementation
 */

void ASIO2WASAPI2::getDriverName(char *name) {
    strcpy_s(name, 32, "ASIO2WASAPI2");
}

long ASIO2WASAPI2::getDriverVersion() {
    return 1;
}

void ASIO2WASAPI2::getErrorMessage(char *s) {
    // TODO: maybe add useful message
    s[0] = 0;
}

ASIOError ASIO2WASAPI2::future(long selector, void *opt) {
    // none of the optional features are present
    return ASE_NotPresent;
}

ASIOError ASIO2WASAPI2::outputReady() {
    // No latency reduction can be achieved, return ASE_NotPresent
    return ASE_NotPresent;
}

ASIOError ASIO2WASAPI2::getChannels(long *numInputChannels, long *numOutputChannels) {
    if (!m_initialized)
        return ASE_NotPresent;

    if (numInputChannels)
        *numInputChannels = 0;
    if (numOutputChannels)
        *numOutputChannels = m_settings.nChannels;
    return ASE_OK;
}


ASIOBool ASIO2WASAPI2::init(void *sysRef) {
    LOGGER_TRACE_FUNC;

    if (m_initialized)
        return true;

    m_hAppWindowHandle = (HWND) sysRef;

    HRESULT hr = S_OK;
    Logger::info(L"ASIO2WASAPI2 initializing");

    CoInitialize(nullptr);

    bool bDeviceFound = false;
    int deviceIndex = 0;
    const auto &targetDeviceId = m_settings.deviceId;
    if (!targetDeviceId.empty()) {
        iterateAudioEndPoints([&](auto pMMDevice) {
            auto deviceId = getDeviceId(pMMDevice);
            auto friendlyName = getDeviceFriendlyName(pMMDevice);
            Logger::debug(L" - Device #%d: %ws (%ws)", deviceIndex++, friendlyName.c_str(), deviceId.c_str());
            if (deviceId == targetDeviceId) {
                Logger::info(L"Found the device");
                m_pDevice = pMMDevice;
                bDeviceFound = true;
                return false;
            }
            return true;
        });
    }

    if (!bDeviceFound) { // id not found
        Logger::error("Target device not found: %ws", targetDeviceId.c_str());
        return false;
    }

    Logger::debug(L"Searching available stream format for device");
    Logger::debug(L" - Target: %d channels, sample rate %d", m_settings.nChannels, m_settings.nSampleRate);
    BOOL rc = FindStreamFormat(m_pDevice, m_settings.nChannels, m_settings.nSampleRate);
    if (!rc) { // go through all devices and try to find the one that works for 16/48K
        Logger::error(L"Specified device doesn't support specified stream format");
        return false;
    }

    m_initialized = true;

    return true;
}

ASIOError ASIO2WASAPI2::getSampleRate(ASIOSampleRate *sampleRate) {
    if (!sampleRate)
        return ASE_InvalidParameter;
    if (!m_initialized)
        return ASE_NotPresent;
    *sampleRate = m_settings.nSampleRate;

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::setSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    Logger::debug(L"setSampleRate: %f", sampleRate);

    if (!m_initialized)
        return ASE_NotPresent;

    if (sampleRate == m_settings.nSampleRate)
        return ASE_OK;

    ASIOError err = canSampleRate(sampleRate);
    Logger::debug(L"canSampleRate: %d", err);
    if (err != ASE_OK)
        return err;

    int nPrevSampleRate = m_settings.nSampleRate;
    m_settings.nSampleRate = (int) sampleRate;
    settingsWriteToRegistry();  // new nSampleRate used here
    if (m_callbacks) { // ask the host ro reset us
        m_settings.nSampleRate = nPrevSampleRate;
        m_callbacks->asioMessage(kAsioResetRequest, 0, nullptr, nullptr);
    } else { // reinitialize us with the new sample rate
        HWND hAppWindowHandle = m_hAppWindowHandle;
        shutdown();
        settingsReadFromRegistry();
        init(hAppWindowHandle);
    }

    return ASE_OK;
}

// all buffer sizes are in frames
ASIOError ASIO2WASAPI2::getBufferSize(long *minSize, long *maxSize,
                                      long *preferredSize, long *granularity) {
    if (!m_initialized)
        return ASE_NotPresent;

    if (minSize)
        *minSize = 32;
    if (maxSize)
        *maxSize = 1024;
    if (preferredSize)
        *preferredSize = 1024;
    if (granularity)
        *granularity = 0;

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::createBuffers(
        ASIOBufferInfo *bufferInfos,
        long numChannels,
        long bufferSize,
        ASIOCallbacks *callbacks) {

    if (!m_initialized)
        return ASE_NotPresent;

    LOGGER_TRACE_FUNC;

    // Check buffers
    if (!callbacks) return ASE_InvalidParameter;
    if (numChannels < 0 || numChannels > m_settings.nChannels) return ASE_InvalidParameter;
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= m_settings.nChannels)
            return ASE_InvalidMode;
    }

    // dispose exiting buffers
    disposeBuffers();

    // Allocate!
    m_bufferSize = bufferSize;
    m_callbacks = callbacks;
    m_buffers[0].resize(m_settings.nChannels);
    m_buffers[1].resize(m_settings.nChannels);
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        m_buffers[0].at(info.channelNum).resize(bufferSize);
        m_buffers[1].at(info.channelNum).resize(bufferSize);
        info.buffers[0] = m_buffers[0].at(info.channelNum).data();
        info.buffers[1] = m_buffers[0].at(info.channelNum).data();
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI2::disposeBuffers() {
    LOGGER_TRACE_FUNC;
    stop();

    // wait for the play thread to finish
    m_output = nullptr;

    m_buffers[0].clear();
    m_buffers[1].clear();
    return ASE_OK;
}

const char *knownChannelNames[] =
        {
                "Front left",
                "Front right",
                "Front center",
                "Low frequency",
                "Back left",
                "Back right",
                "Front left of center",
                "Front right of center",
                "Back center",
                "Side left",
                "Side right",
        };

ASIOError ASIO2WASAPI2::getChannelInfo(ASIOChannelInfo *info) {
    if (!m_initialized) return ASE_NotPresent;
    if (info->isInput) return ASE_InvalidParameter;
    if (info->channel < 0 || info->channel >= m_settings.nChannels) return ASE_InvalidParameter;

    info->type = sampleType;
    info->channelGroup = 0;
    info->isActive = (!m_buffers[0].empty()) ? ASIOTrue : ASIOFalse;

    strcpy_s(info->name, sizeof(info->name),
             (info->channel < sizeof(knownChannelNames) / sizeof(knownChannelNames[0]))
             ? knownChannelNames[info->channel]
             : "Unknown");

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::canSampleRate(ASIOSampleRate sampleRate) {
    if (!m_initialized)
        return ASE_NotPresent;

    int nSampleRate = static_cast<int>(sampleRate);
    return FindStreamFormat(m_pDevice, m_settings.nChannels, nSampleRate) ? ASE_OK : ASE_NoClock;
}

ASIOError ASIO2WASAPI2::start() {
    LOGGER_TRACE_FUNC;

    if (!m_initialized || !m_callbacks)
        return ASE_NotPresent;
    if (m_output)
        return ASE_OK; // we are already playing

    // make sure the previous play thread exited
    m_samplePosition = 0;
    m_output = std::make_unique<WASAPIOutput>(
            m_pDevice,
            m_settings.nChannels,
            m_settings.nSampleRate,
            m_bufferSize);
    m_output->registerCallback([&]() { this->pushData(); });

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::stop() {
    LOGGER_TRACE_FUNC;

    if (!m_initialized)
        return ASE_NotPresent;
    if (!m_output)
        return ASE_OK; // we already stopped

    m_output = nullptr;
    return ASE_OK;
}

void ASIO2WASAPI2::pushData() {
    if (m_callbacks) {
        auto nowEmptyBufferIndex = m_bufferIndex;
        m_bufferIndex = 1 - m_bufferIndex;
        auto &buffer = m_buffers[m_bufferIndex];
        m_output->pushSamples(buffer);
        m_callbacks->bufferSwitch(nowEmptyBufferIndex, ASIOTrue);
    }
}


ASIOError ASIO2WASAPI2::getClockSources(ASIOClockSource *clocks, long *numSources) {
    if (!numSources || *numSources == 0)
        return ASE_OK;
    clocks->index = 0;
    clocks->associatedChannel = -1;
    clocks->associatedGroup = -1;
    clocks->isCurrentSource = ASIOTrue;
    strcpy_s(clocks->name, "Internal clock");
    *numSources = 1;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2::setClockSource(long index) {
    LOGGER_TRACE_FUNC;

    return (index == 0) ? ASE_OK : ASE_NotPresent;
}

ASIOError ASIO2WASAPI2::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp) {
    if (!m_initialized || !m_callbacks)
        return ASE_NotPresent;
    if (tStamp) {
        tStamp->lo = m_theSystemTime.lo;
        tStamp->hi = m_theSystemTime.hi;
    }
    if (sPos) {
        if (m_samplePosition >= twoRaisedTo32) {
            sPos->hi = (unsigned long) (m_samplePosition * twoRaisedTo32Reciprocal);
            sPos->lo = (unsigned long) (m_samplePosition - (sPos->hi * twoRaisedTo32));
        } else {
            sPos->hi = 0;
            sPos->lo = (unsigned long) m_samplePosition;
        }
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI2::getLatencies(long *_inputLatency, long *_outputLatency) {
    if (!m_initialized || !m_callbacks)
        return ASE_NotPresent;
    if (_inputLatency)
        *_inputLatency = m_bufferSize;
    if (_outputLatency)
        *_outputLatency = 2 * m_bufferSize;
    return ASE_OK;
}

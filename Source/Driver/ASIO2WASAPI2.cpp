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
#include <codecvt> // codecvt_utf8
#include <locale>  // wstring_convert
#include <timeapi.h>
#include "ASIO2WASAPI2.h"
#include "resource.h"
#include "logger.h"
#include "json.hpp"
#include "WASAPIOutput/createIAudioClient.h"
#include "WASAPIUtils.h"

using json = nlohmann::json;

CLSID CLSID_ASIO2WASAPI2_DRIVER = {0xe3226090, 0x473d, 0x4cc9, {0x83, 0x60, 0xe1, 0x23, 0xeb, 0x9e, 0xf8, 0x47}};

const TCHAR *szPrefsRegKey = TEXT("Software\\ASIO2WASAPI2");

template<typename T>
auto make_autorelease(T *ptr) {
    return std::shared_ptr<T>(ptr, [](T *p) {
        if (p) p->Release();
    });
}

template<typename T>
auto make_autoclose(T *h) {
    return std::shared_ptr<T>(h, [](T *h) {
        if (h) CloseHandle(h);
    });
}

inline long ASIO2WASAPI2::refTimeToBufferSize(REFERENCE_TIME time) const {
    const double REFTIME_UNITS_PER_SECOND = 10000000.;
    return static_cast<long>(ceil(m_nSampleRate * (time / REFTIME_UNITS_PER_SECOND)));
}

inline REFERENCE_TIME ASIO2WASAPI2::bufferSizeToRefTime(long bufferSize) const {
    const double REFTIME_UNITS_PER_SECOND = 10000000.;
    return static_cast<REFERENCE_TIME>(ceil(bufferSize / (m_nSampleRate / REFTIME_UNITS_PER_SECOND)));
}

const double twoRaisedTo32 = 4294967296.;
const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

inline void getNanoSeconds(ASIOTimeStamp *ts) {
    double nanoSeconds = (double) ((unsigned long) timeGetTime()) * 1000000.;
    ts->hi = (unsigned long) (nanoSeconds / twoRaisedTo32);
    ts->lo = (unsigned long) (nanoSeconds - (ts->hi * twoRaisedTo32));
}


CUnknown *ASIO2WASAPI2::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr) {
    LOGGER_TRACE_FUNC;
    return static_cast<CUnknown *>(new ASIO2WASAPI2(pUnk, phr));
};

STDMETHODIMP ASIO2WASAPI2::NonDelegatingQueryInterface(REFIID riid, void **ppv) {
    LOGGER_TRACE_FUNC;
    if (riid == CLSID_ASIO2WASAPI2_DRIVER) {
        return GetInterface(this, ppv);
    }
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
}

ASIOSampleType ASIO2WASAPI2::getASIOSampleType() const {
    return ASIOSTInt16LSB;
}

const TCHAR *szJsonRegValName = TEXT("json");

// convert UTF-8 string to std::wstring
std::wstring utf8_to_wstring(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

// convert std::wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

void ASIO2WASAPI2::readFromRegistry() {
    LOGGER_TRACE_FUNC;
    Logger::debug(L"ASIO2WASAPI2::readFromRegistery");
    HKEY key = 0;
    LONG lResult = RegOpenKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, KEY_READ, &key);
    if (ERROR_SUCCESS == lResult) {
        DWORD size;

        RegGetValue(key, NULL, szJsonRegValName, RRF_RT_REG_SZ, NULL, NULL, &size);
        if (size) {
            std::vector<BYTE> v(size);
            RegGetValue(key, NULL, szJsonRegValName, RRF_RT_REG_SZ, NULL, v.data(), &size);
            try {
                json j = json::parse(v.begin(), v.end());
                int nSampleRate = j["nSampleRate"];
                int nChannels = j["nChannels"];
                std::wstring deviceId = utf8_to_wstring(j["deviceId"]);

                m_nSampleRate = nSampleRate;
                m_nChannels = nChannels;
                m_deviceId = deviceId;
                Logger::debug(L" - m_nChannels: %d", m_nChannels);
                Logger::debug(L" - m_nSampleRate: %d", m_nSampleRate);
                Logger::debug(L" - m_deviceId: %ws", m_deviceId.c_str());
            }
            catch (json::exception &e) {
                Logger::error(L"JSON error: %s", e.what());
            }
        }
        RegCloseKey(key);
    }
}

void ASIO2WASAPI2::writeToRegistry() {
    LOGGER_TRACE_FUNC;
    HKEY key = 0;
    LONG lResult = RegCreateKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    if (ERROR_SUCCESS == lResult) {
        json j = {
                {"nChannels",   m_nChannels},
                {"nSampleRate", m_nSampleRate},
                {"deviceId",    wstring_to_utf8(m_deviceId)}};
        auto jsonString = j.dump();
        RegSetValueEx(key, szJsonRegValName, NULL, REG_SZ, (const BYTE *) jsonString.data(), (DWORD) jsonString.size());
        RegCloseKey(key);
        Logger::debug(L" - m_nChannels: %d", m_nChannels);
        Logger::debug(L" - m_nSampleRate: %d", m_nSampleRate);
        Logger::debug(L" - m_deviceId: %ws", &m_deviceId[0]);
    }
}

void ASIO2WASAPI2::clearState() {
    LOGGER_TRACE_FUNC;

    // For safety
    if (m_output) {
        Logger::error(L"m_output still not NULL on clearState");
        m_output = NULL;
    }

    // fields valid before initialization
    m_nChannels = 2;
    m_nSampleRate = 48000;
    m_deviceId.clear();

    // fields filled by init()/cleaned by shutdown()
    m_active = false;
    m_pDevice = NULL;
    m_bufferIndex = 0;
    m_hAppWindowHandle = NULL;

    // fields filled by createBuffers()/cleaned by disposeBuffers()
    m_buffers[0].clear();
    m_buffers[1].clear();
    m_callbacks = NULL;

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
    readFromRegistry();

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

BOOL CALLBACK ASIO2WASAPI2::ControlPanelProc(HWND hwndDlg,
                                             UINT message, WPARAM wParam, LPARAM lParam) {
    static ASIO2WASAPI2 *pDriver = NULL;
    static std::vector<std::wstring> deviceStringIds;
    switch (message) {
        case WM_DESTROY:
            pDriver = NULL;
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
                        std::shared_ptr<IMMDevice> pDevice = NULL;
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
                                    m_pCallbacks->asioMessage(kAsioResetRequest, 0, NULL, NULL);
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
                        pDriver->m_nSampleRate = nSampleRate;
                        pDriver->m_nChannels = nChannels;
                        pDriver->m_deviceId = selectedDeviceId;

                        // try to init the driver
                        if (pDriver->init(hAppWindowHandle) == ASIOFalse) {
                            MessageBox(hwndDlg, TEXT("ASIO driver failed to initialize"), szDescription, MB_OK);
                            return 0;
                        }
                        pDriver->writeToRegistry();
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
            LOGGER_TRACE_FUNC;
            pDriver = (ASIO2WASAPI2 *) lParam;
            if (!pDriver)
                return FALSE;
            SetDlgItemInt(hwndDlg, IDC_CHANNELS, (UINT) pDriver->m_nChannels, TRUE);
            SetDlgItemInt(hwndDlg, IDC_SAMPLE_RATE, (UINT) pDriver->m_nSampleRate, TRUE);

            CoInitialize(NULL);

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
            int nDeviceIdIndex = -1;
            if (pDriver->m_deviceId.size())
                for (unsigned i = 0; i < deviceStringIds.size(); i++) {
                    if (deviceStringIds[i] == pDriver->m_deviceId) {
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
    if (!m_active)
        return ASE_NotPresent;

    if (numInputChannels)
        *numInputChannels = 0;
    if (numOutputChannels)
        *numOutputChannels = m_nChannels;
    return ASE_OK;
}

ASIOError ASIO2WASAPI2::controlPanel() {
    LOGGER_TRACE_FUNC;

    extern HINSTANCE g_hinstDLL;
    DialogBoxParam(g_hinstDLL, MAKEINTRESOURCE(IDD_CONTROL_PANEL), m_hAppWindowHandle, (DLGPROC) ControlPanelProc,
                   (LPARAM) this);
    return ASE_OK;
}

ASIOBool ASIO2WASAPI2::init(void *sysRef) {
    LOGGER_TRACE_FUNC;

    if (m_active)
        return true;

    m_hAppWindowHandle = (HWND) sysRef;

    HRESULT hr = S_OK;
    Logger::info(L"ASIO2WASAPI2 initializing");

    CoInitialize(NULL);

    bool bDeviceFound = false;
    int deviceIndex = 0;
    iterateAudioEndPoints([&](auto pMMDevice) {
        auto deviceId = getDeviceId(pMMDevice);
        Logger::debug(L" - Device #%d: %ws", deviceIndex++, deviceId.c_str());
        if (deviceId.size() && m_deviceId.size() && deviceId == m_deviceId) {
            Logger::info(L"Found the device");
            m_pDevice = pMMDevice;
            m_pDevice->AddRef();
            bDeviceFound = true;
            return false;
        }
        return true;
    });

    if (!bDeviceFound) { // id not found
        Logger::error(L"Target device not found: %ws", m_deviceId.c_str());
        return false;
    }

    m_deviceId = getDeviceId(m_pDevice);

    Logger::debug(L"Searching available stream format for device");
    Logger::debug(L" - Target: %d channels, sample rate %d", m_nChannels, m_nSampleRate);
    BOOL rc = FindStreamFormat(m_pDevice, m_nChannels, m_nSampleRate);
    if (!rc) { // go through all devices and try to find the one that works for 16/48K
        Logger::error(L"Specified device doesn't support specified stream format");
        return false;
    }

    m_active = true;

    return true;
}

ASIOError ASIO2WASAPI2::getSampleRate(ASIOSampleRate *sampleRate) {
    if (!sampleRate)
        return ASE_InvalidParameter;
    if (!m_active)
        return ASE_NotPresent;
    *sampleRate = m_nSampleRate;

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::setSampleRate(ASIOSampleRate sampleRate) {
    LOGGER_TRACE_FUNC;

    Logger::debug(L"setSampleRate: %f", sampleRate);

    if (!m_active)
        return ASE_NotPresent;

    if (sampleRate == m_nSampleRate)
        return ASE_OK;

    ASIOError err = canSampleRate(sampleRate);
    Logger::debug(L"canSampleRate: %d", err);
    if (err != ASE_OK)
        return err;

    int nPrevSampleRate = m_nSampleRate;
    m_nSampleRate = (int) sampleRate;
    writeToRegistry();
    if (m_callbacks) { // ask the host ro reset us
        m_nSampleRate = nPrevSampleRate;
        m_callbacks->asioMessage(kAsioResetRequest, 0, NULL, NULL);
    } else { // reinitialize us with the new sample rate
        HWND hAppWindowHandle = m_hAppWindowHandle;
        shutdown();
        readFromRegistry();
        init(hAppWindowHandle);
    }

    return ASE_OK;
}

// all buffer sizes are in frames
ASIOError ASIO2WASAPI2::getBufferSize(long *minSize, long *maxSize,
                                      long *preferredSize, long *granularity) {
    if (!m_active)
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

ASIOError ASIO2WASAPI2::createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
                                      long bufferSize, ASIOCallbacks *callbacks) {
    LOGGER_TRACE_FUNC;

    if (!m_active)
        return ASE_NotPresent;

    // some sanity checks
    if (!callbacks || numChannels < 0 || numChannels > m_nChannels)
        return ASE_InvalidParameter;
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= m_nChannels)
            return ASE_InvalidMode;
    }

    // dispose exiting buffers
    disposeBuffers();

    m_bufferSize = bufferSize;
    m_callbacks = callbacks;
    int sampleContainerLength = 2;
    int bufferByteLength = bufferSize * sampleContainerLength;

    // the very allocation
    m_buffers[0].resize(m_nChannels);
    m_buffers[1].resize(m_nChannels);

    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        m_buffers[0].at(info.channelNum).resize(bufferByteLength);
        m_buffers[1].at(info.channelNum).resize(bufferByteLength);
        info.buffers[0] = &m_buffers[0].at(info.channelNum)[0];
        info.buffers[1] = &m_buffers[1].at(info.channelNum)[0];
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

ASIOError ASIO2WASAPI2::getChannelInfo(ASIOChannelInfo *info) {
    if (!m_active)
        return ASE_NotPresent;

    if (info->channel < 0 || info->channel >= m_nChannels || info->isInput)
        return ASE_InvalidParameter;

    info->type = getASIOSampleType();
    info->channelGroup = 0;
    info->isActive = (m_buffers[0].size() > 0) ? ASIOTrue : ASIOFalse;
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

    strcpy_s(info->name, sizeof(info->name),
             (info->channel < sizeof(knownChannelNames) / sizeof(knownChannelNames[0]))
             ? knownChannelNames[info->channel]
             : "Unknown");

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::canSampleRate(ASIOSampleRate sampleRate) {
    if (!m_active)
        return ASE_NotPresent;

    int nSampleRate = static_cast<int>(sampleRate);
    return FindStreamFormat(m_pDevice, m_nChannels, nSampleRate) ? ASE_OK : ASE_NoClock;
}

ASIOError ASIO2WASAPI2::start() {
    LOGGER_TRACE_FUNC;

    if (!m_active || !m_callbacks)
        return ASE_NotPresent;
    if (m_output)
        return ASE_OK; // we are already playing

    // make sure the previous play thread exited
    m_output = std::make_unique<WASAPIOutput>(m_pDevice, m_nChannels, m_nSampleRate);
    return ASE_OK;
}

ASIOError ASIO2WASAPI2::stop() {
    LOGGER_TRACE_FUNC;

    if (!m_active)
        return ASE_NotPresent;
    if (!m_output)
        return ASE_OK; // we already stopped

    m_output = nullptr;
    return ASE_OK;
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
    if (!m_active || !m_callbacks)
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
    if (!m_active || !m_callbacks)
        return ASE_NotPresent;
    if (_inputLatency)
        *_inputLatency = m_bufferSize;
    if (_outputLatency)
        *_outputLatency = 2 * m_bufferSize;
    return ASE_OK;
}

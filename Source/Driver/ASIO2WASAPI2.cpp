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
#include <mmsystem.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include <codecvt> // codecvt_utf8
#include <locale>  // wstring_convert
#include "Avrt.h" //used for AvSetMmThreadCharacteristics
#include <Functiondiscoverykeys_devpkey.h>
#include "ASIO2WASAPI2.h"
#include "resource.h"
#include "logger.h"
#include "json.hpp"

using json = nlohmann::json;

CLSID CLSID_ASIO2WASAPI2_DRIVER = {0xe3226090, 0x473d, 0x4cc9, {0x83, 0x60, 0xe1, 0x23, 0xeb, 0x9e, 0xf8, 0x47}};

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

const TCHAR *szPrefsRegKey = TEXT("Software\\ASIO2WASAPI2");

template<typename T>
auto make_autorelease(T *ptr) {
    return shared_ptr<T>(ptr, [](T *p) {
        if (p) p->Release();
    });
}

template<typename T>
auto make_autoclose(T *h) {
    return shared_ptr<T>(h, [](T *h) {
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

inline wstring getDeviceId(shared_ptr<IMMDevice> pDevice) {
    wstring id;
    LPWSTR pDeviceId = NULL;
    HRESULT hr = pDevice->GetId(&pDeviceId);
    if (FAILED(hr))
        return id;
    id = pDeviceId;
    CoTaskMemFree(pDeviceId);
    pDeviceId = NULL;
    return id;
}

bool iterateAudioEndPoints(std::function<bool(shared_ptr<IMMDevice> pMMDevice)> cb) {
    IMMDeviceEnumerator *pEnumerator_ = NULL;
    DWORD flags = 0;

    HRESULT hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator, NULL,
            CLSCTX_ALL, IID_IMMDeviceEnumerator,
            (void **) &pEnumerator_);
    if (FAILED(hr))
        return false;

    auto pEnumerator = make_autorelease(pEnumerator_);

    IMMDeviceCollection *pMMDeviceCollection_ = NULL;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection_);
    if (FAILED(hr))
        return false;
    auto pMMDeviceCollection = make_autorelease(pMMDeviceCollection_);

    UINT nDevices = 0;
    hr = pMMDeviceCollection->GetCount(&nDevices);
    if (FAILED(hr))
        return false;

    for (UINT i = 0; i < nDevices; i++) {
        IMMDevice *pMMDevice_ = NULL;
        hr = pMMDeviceCollection->Item(i, &pMMDevice_);
        if (FAILED(hr))
            return false;
        auto pMMDevice = make_autorelease(pMMDevice_);

        if (!cb(pMMDevice))
            break;
    }
    return true;
}

shared_ptr<IAudioClient> getAudioClient(shared_ptr<IMMDevice> pDevice, WAVEFORMATEX *pWaveFormat) {
    if (!pDevice || !pWaveFormat)
        return NULL;

    IAudioClient *pAudioClient_ = NULL;
    HRESULT hr = pDevice->Activate(
            IID_IAudioClient, CLSCTX_ALL,
            NULL, (void **) &pAudioClient_);
    if (FAILED(hr) || !pAudioClient_)
        return NULL;
    auto pAudioClient = make_autorelease(pAudioClient_);

    hr = pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, pWaveFormat, NULL);
    if (FAILED(hr))
        return NULL;

    // calculate buffer size and duration
    REFERENCE_TIME hnsDefaultDuration = 0;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDuration, NULL);
    if (FAILED(hr)) return NULL;

    hnsDefaultDuration = max(hnsDefaultDuration, (REFERENCE_TIME) 1000000); // 100ms minimum

    hr = pAudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            hnsDefaultDuration,
            hnsDefaultDuration,
            pWaveFormat,
            NULL);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        UINT bufferSize = 0;
        hr = pAudioClient->GetBufferSize(&bufferSize);
        if (FAILED(hr)) {
            Logger::error(L"pAudioClient->GetBufferSize failed");
            return NULL;
        }

        const double REFTIME_UNITS_PER_SECOND = 10000000.;
        REFERENCE_TIME hnsAlignedDuration = static_cast<REFERENCE_TIME>(ceil(
                bufferSize / (pWaveFormat->nSamplesPerSec / REFTIME_UNITS_PER_SECOND)));
        hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void **) &pAudioClient_);
        if (FAILED(hr) || !pAudioClient_) return NULL;
        pAudioClient.reset(pAudioClient_);

        hr = pAudioClient->Initialize(
                AUDCLNT_SHAREMODE_EXCLUSIVE,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                hnsAlignedDuration,
                hnsAlignedDuration,
                pWaveFormat,
                NULL);
    }
    if (FAILED(hr))
        return NULL;
    return pAudioClient;
}

BOOL FindStreamFormat(shared_ptr<IMMDevice> pDevice, int nChannels, int nSampleRate, WAVEFORMATEXTENSIBLE *pwfxt = NULL,
                      shared_ptr<IAudioClient> *ppAudioClient = NULL) {
    LOGGER_TRACE_FUNC;

    if (!pDevice)
        return FALSE;

    // create a reasonable channel mask
    DWORD dwChannelMask = 0;
    DWORD bit = 1;
    for (int i = 0; i < nChannels; i++) {
        dwChannelMask |= bit;
        bit <<= 1;
    }

    WAVEFORMATEXTENSIBLE waveFormat;

    // try 32-bit first
    Logger::debug(L"Trying 32-bit stream");
    waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    waveFormat.Format.nChannels = nChannels;
    waveFormat.Format.nSamplesPerSec = nSampleRate;
    waveFormat.Format.wBitsPerSample = 32;
    waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels / 8;
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    waveFormat.Samples.wValidBitsPerSample = waveFormat.Format.wBitsPerSample;
    waveFormat.dwChannelMask = dwChannelMask;
    waveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    auto pAudioClient = getAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat);

    if (pAudioClient) {
        Logger::debug(L" - works!");
        goto Finish;
    }

    // try 24-bit containered next
    Logger::debug(L"Trying 24-bit containered");
    waveFormat.Samples.wValidBitsPerSample = 24;

    pAudioClient = getAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat);

    if (pAudioClient) {
        Logger::debug(L" - works!");
        goto Finish;
    }

    // try 24-bit packed next
    Logger::debug(L"Trying 24-bit packed");
    waveFormat.Format.wBitsPerSample = 24;
    waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels / 8;
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Samples.wValidBitsPerSample = waveFormat.Format.wBitsPerSample;

    pAudioClient = getAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat);

    if (pAudioClient) {
        Logger::debug(L" - works!");
        goto Finish;
    }

    // finally, try 16-bit
    Logger::debug(L"Trying 16-bit packed");
    waveFormat.Format.wBitsPerSample = 16;
    waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels / 8;
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Samples.wValidBitsPerSample = waveFormat.Format.wBitsPerSample;

    pAudioClient = getAudioClient(pDevice, (WAVEFORMATEX *) &waveFormat);
    if (pAudioClient) {
        Logger::debug(L" - works!");
        goto Finish;
    }

    Logger::debug(L" - none works");

    Finish:
    BOOL bSuccess = (pAudioClient != NULL);
    if (bSuccess) {
        if (pwfxt)
            memcpy_s(pwfxt, sizeof(WAVEFORMATEXTENSIBLE), &waveFormat, sizeof(WAVEFORMATEXTENSIBLE));
        if (ppAudioClient)
            *ppAudioClient = pAudioClient;
    }
    return bSuccess;
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
    LOGGER_TRACE_FUNC;
    switch (m_waveFormat.Format.wBitsPerSample) {
        case 16:
            return ASIOSTInt16LSB;
        case 24:
            return ASIOSTInt24LSB;
        case 32:
            switch (m_waveFormat.Samples.wValidBitsPerSample) {
                case 32:
                    return ASIOSTInt32LSB;
                case 24:
                    return ASIOSTInt32LSB24;
                default:
                    return ASIOSTLastEntry;
            }
        default:
            return ASIOSTLastEntry;
    }
}

const TCHAR *szJsonRegValName = TEXT("json");

// convert UTF-8 string to wstring
std::wstring utf8_to_wstring(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

// convert wstring to UTF-8 string
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
    // fields valid before initialization
    m_nChannels = 2;
    m_nSampleRate = 48000;
    m_deviceId.clear();
    m_hStopPlayThreadEvent = NULL;

    // fields filled by init()/cleaned by shutdown()
    m_active = false;
    m_pDevice = NULL;
    m_pAudioClient = NULL;
    memset(&m_waveFormat, 0, sizeof(m_waveFormat));
    m_bufferIndex = 0;
    m_hAppWindowHandle = NULL;

    // fields filled by createBuffers()/cleaned by disposeBuffers()
    m_buffers[0].clear();
    m_buffers[1].clear();
    m_callbacks = NULL;

    // fields filled by start()/cleaned by stop()
    m_hPlayThreadIsRunningEvent = NULL;
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
    static vector<wstring> deviceStringIds;
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
                        shared_ptr<IMMDevice> pDevice = NULL;
                        {

                            iterateAudioEndPoints([&](shared_ptr<IMMDevice> pMMDevice) {
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

            vector<wstring> deviceIds;
            if (!iterateAudioEndPoints([&](auto &pMMDevice) {
                auto deviceId = getDeviceId(pMMDevice);
                if (deviceId.size() == 0)
                    return false;
                deviceIds.push_back(deviceId);

                IPropertyStore *pPropertyStore_;
                HRESULT hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore_);
                if (FAILED(hr))
                    return false;
                auto pPropertyStore = make_autorelease(pPropertyStore_);

                PROPVARIANT var;
                PropVariantInit(&var);
                hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &var);
                if (FAILED(hr))
                    return false;
                LRESULT lr = 0;
                if (var.vt != VT_LPWSTR ||
                    (lr = SendDlgItemMessageW(hwndDlg, IDL_DEVICE, LB_ADDSTRING, -1, (LPARAM) var.pwszVal)) == CB_ERR) {
                    PropVariantClear(&var);
                    return false;
                }
                PropVariantClear(&var);
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

#define RETURN_ON_ERROR(hres) \
    if (FAILED(hres))         \
        return -1;

DWORD WINAPI ASIO2WASAPI2::PlayThreadProc(LPVOID pThis) {
    ASIO2WASAPI2 *pDriver = static_cast<ASIO2WASAPI2 *>(pThis);
    struct CExitEventSetter {
        HANDLE &m_hEvent;

        CExitEventSetter(ASIO2WASAPI2 *pDriver) : m_hEvent(pDriver->m_hPlayThreadIsRunningEvent) {
            m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }

        ~CExitEventSetter() {
            SetEvent(m_hEvent);
            CloseHandle(m_hEvent);
            m_hEvent = NULL;
        }
    } setter(pDriver);

    HRESULT hr = S_OK;

    auto pAudioClient = pDriver->m_pAudioClient;
    BYTE *pData = NULL;

    hr = CoInitialize(NULL);
    RETURN_ON_ERROR(hr)

    // Create an event handle and register it for
    // buffer-event notifications.
    HANDLE hEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    auto hEvent = make_autoclose(hEvent_);

    hr = pAudioClient->SetEventHandle(hEvent.get());
    RETURN_ON_ERROR(hr)

    IAudioRenderClient *pRenderClient_ = NULL;
    hr = pAudioClient->GetService(
            IID_IAudioRenderClient,
            (void **) &pRenderClient_);
    RETURN_ON_ERROR(hr)
    auto pRenderClient = make_autorelease(pRenderClient_);

    // Ask MMCSS to temporarily boost the thread priority
    // to reduce the possibility of glitches while we play.
    DWORD taskIndex = 0;
    AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);

    // Pre-load the first buffer with data
    // from the audio source before starting the stream.
    hr = pDriver->LoadData(pRenderClient);
    RETURN_ON_ERROR(hr)

    hr = pAudioClient->Start(); // Start playing.
    RETURN_ON_ERROR(hr)

    getNanoSeconds(&pDriver->m_theSystemTime);
    pDriver->m_samplePosition = 0;

    if (pDriver->m_callbacks)
        pDriver->m_callbacks->bufferSwitch(1 - pDriver->m_bufferIndex, ASIOTrue);

    DWORD retval = 0;
    HANDLE events[2] = {pDriver->m_hStopPlayThreadEvent, hEvent.get()};
    while ((retval = WaitForMultipleObjects(2, events, FALSE, INFINITE)) ==
           (WAIT_OBJECT_0 + 1)) { // the hEvent is signalled and m_hStopPlayThreadEvent is not
        // Grab the next empty buffer from the audio device.
        hr = pDriver->LoadData(pRenderClient);
        getNanoSeconds(&pDriver->m_theSystemTime);
        pDriver->m_samplePosition += pDriver->m_bufferSize;
        if (pDriver->m_callbacks)
            pDriver->m_callbacks->bufferSwitch(1 - pDriver->m_bufferIndex, ASIOTrue);
    }

    hr = pAudioClient->Stop(); // Stop playing.
    RETURN_ON_ERROR(hr)
    pDriver->m_samplePosition = 0;

    return 0;
}

#undef RETURN_ON_ERROR

HRESULT ASIO2WASAPI2::LoadData(shared_ptr<IAudioRenderClient> pRenderClient) {
    if (!pRenderClient)
        return E_INVALIDARG;

    HRESULT hr = S_OK;
    BYTE *pData = NULL;
    hr = pRenderClient->GetBuffer(m_bufferSize, &pData);

    UINT32 sampleSize = m_waveFormat.Format.wBitsPerSample / 8;

    // switch buffer
    m_bufferIndex = 1 - m_bufferIndex;
    vector<vector<BYTE>> &buffer = m_buffers[m_bufferIndex];
    unsigned sampleOffset = 0;
    unsigned nextSampleOffset = sampleSize;
    for (int i = 0; i < m_bufferSize; i++, sampleOffset = nextSampleOffset, nextSampleOffset += sampleSize) {
        for (unsigned j = 0; j < buffer.size(); j++) {
            if (buffer[j].size() >= nextSampleOffset)
                memcpy_s(pData, sampleSize, &buffer[j].at(0) + sampleOffset, sampleSize);
            else
                memset(pData, 0, sampleSize);
            pData += sampleSize;
        }
    }

    hr = pRenderClient->ReleaseBuffer(m_bufferSize, 0);

    return S_OK;
}

/*  ASIO driver interface implementation
 */

void ASIO2WASAPI2::getDriverName(char *name) {
    strcpy_s(name, 32, "ASIO2WASAPI2");
}

long ASIO2WASAPI2::getDriverVersion() {
    return 1;
}

void ASIO2WASAPI2::getErrorMessage(char *string) {
    // TODO: maybe add useful message
    string[0] = 0;
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

void ASIO2WASAPI2::setMostReliableFormat() {
    LOGGER_TRACE_FUNC;

    m_nChannels = 2;
    m_nSampleRate = 48000;
    memset(&m_waveFormat, 0, sizeof(m_waveFormat));
    WAVEFORMATEX &fmt = m_waveFormat.Format;
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 48000;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.wBitsPerSample = 16;
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
    BOOL rc = FindStreamFormat(m_pDevice, m_nChannels, m_nSampleRate, &m_waveFormat, &m_pAudioClient);
    if (!rc) { // go through all devices and try to find the one that works for 16/48K
        Logger::error(L"Specified device doesn't support specified stream format");
        return false;
    }

    UINT32 bufferSize = 0;
    hr = m_pAudioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr))
        return false;

    m_bufferSize = bufferSize;
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

    Logger::debug(L"setSampleRate: %lf", sampleRate);

    if (!m_active)
        return ASE_NotPresent;

    if (sampleRate == m_nSampleRate)
        return ASE_OK;

    ASIOError err = canSampleRate(sampleRate);
    Logger::debug(L"canSampleRate: %ld", err);
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
        *minSize = m_bufferSize;
    if (maxSize)
        *maxSize = m_bufferSize;
    if (preferredSize)
        *preferredSize = m_bufferSize;
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
    if (bufferSize != m_bufferSize)
        return ASE_InvalidMode;
    for (int i = 0; i < numChannels; i++) {
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= m_nChannels)
            return ASE_InvalidMode;
    }

    // dispose exiting buffers
    disposeBuffers();

    m_callbacks = callbacks;
    int sampleContainerLength = m_waveFormat.Format.wBitsPerSample / 8;
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
    WaitForSingleObject(m_hPlayThreadIsRunningEvent, INFINITE);
    m_callbacks = 0;
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
             ? knownChannelNames[info->channel] : "Unknown");

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
    if (m_hStopPlayThreadEvent)
        return ASE_OK; // we are already playing
    // make sure the previous play thread exited
    WaitForSingleObject(m_hPlayThreadIsRunningEvent, INFINITE);

    m_hStopPlayThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    CreateThread(NULL, 0, PlayThreadProc, this, 0, NULL);

    return ASE_OK;
}

ASIOError ASIO2WASAPI2::stop() {
    LOGGER_TRACE_FUNC;

    if (!m_active)
        return ASE_NotPresent;
    if (!m_hStopPlayThreadEvent)
        return ASE_OK; // we already stopped

    // set the thead stopping event, thus initiating the thread termination process
    SetEvent(m_hStopPlayThreadEvent);
    CloseHandle(m_hStopPlayThreadEvent);
    m_hStopPlayThreadEvent = NULL;

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

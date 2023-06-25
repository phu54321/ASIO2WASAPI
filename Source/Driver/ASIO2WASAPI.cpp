/*  ASIO2WASAPI Universal ASIO Driver

    Copyright (C) 2017 Lev Minkovsky
    Copyright (C) 2023 Hyunwoo Park (phu54321@naver.com) - modifications

    This file is part of ASIO2WASAPI.

    ASIO2WASAPI is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    ASIO2WASAPI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with ASIO2WASAPI; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "stdafx.h"
#include <math.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <Mmdeviceapi.h>
#include <Audioclient.h>
#include "Avrt.h" //used for AvSetMmThreadCharacteristics
#include <Functiondiscoverykeys_devpkey.h>
#include "ASIO2WASAPI.h"
#include "resource.h"
#include "logger.h"

CLSID CLSID_ASIO2WASAPI_DRIVER = { 0x3981c4c8, 0xfe12, 0x4b0f, { 0x98, 0xa0, 0xd1, 0xb6, 0x67, 0xbd, 0xa6, 0x15 } };

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

const char * szPrefsRegKey = "Software\\ASIO2WASAPI";

#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

class CReleaser 
{
    IUnknown * m_pUnknown;
public:
    CReleaser(IUnknown * pUnk) : m_pUnknown(pUnk) {}
    void deactivate() {m_pUnknown = NULL;}
    void reset(IUnknown * pUnk) 
    {
        SAFE_RELEASE(m_pUnknown)
        m_pUnknown = pUnk;
    }
    ~CReleaser() 
    {
        SAFE_RELEASE(m_pUnknown)
    }
};

class CHandleCloser
{
    HANDLE m_h;
public:
    CHandleCloser(HANDLE h) : m_h(h) {}
    ~CHandleCloser() 
    {
        if (m_h != NULL)
            CloseHandle(m_h);
    }
};

inline long ASIO2WASAPI::refTimeToBufferSize(REFERENCE_TIME time) const
{
    const double REFTIME_UNITS_PER_SECOND = 10000000.;
    return static_cast<long>(ceil(m_nSampleRate * ( time / REFTIME_UNITS_PER_SECOND ) ));
}

inline REFERENCE_TIME ASIO2WASAPI::bufferSizeToRefTime(long bufferSize) const
{
    const double REFTIME_UNITS_PER_SECOND = 10000000.;
    return static_cast<REFERENCE_TIME>(ceil(bufferSize / (m_nSampleRate / REFTIME_UNITS_PER_SECOND) ));
}

const double twoRaisedTo32 = 4294967296.;
const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

inline void getNanoSeconds (ASIOTimeStamp* ts)
{
    double nanoSeconds = (double)((unsigned long)timeGetTime ()) * 1000000.;
	ts->hi = (unsigned long)(nanoSeconds / twoRaisedTo32);
	ts->lo = (unsigned long)(nanoSeconds - (ts->hi * twoRaisedTo32));
}

inline vector<wchar_t> getDeviceId(IMMDevice * pDevice)
{
    vector<wchar_t> id;
    LPWSTR pDeviceId = NULL;
    HRESULT hr = pDevice->GetId(&pDeviceId);
    if (FAILED(hr))
        return id;
    size_t nDeviceIdLength=wcslen(pDeviceId);
    if (nDeviceIdLength == 0)
        return id;
    id.resize(nDeviceIdLength+1);
    wcscpy_s(&id[0],nDeviceIdLength+1,pDeviceId);
    CoTaskMemFree(pDeviceId);
    pDeviceId = NULL;
    return id;
}

IAudioClient * getAudioClient(IMMDevice * pDevice, WAVEFORMATEX * pWaveFormat)
{
    if (!pDevice || !pWaveFormat)
        return NULL;

    IAudioClient * pAudioClient = NULL;
    HRESULT hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    if (FAILED(hr) || !pAudioClient)
        return NULL;
    CReleaser r(pAudioClient);

    hr=pAudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,pWaveFormat,NULL);
    if (FAILED(hr))
        return NULL;
   
    //calculate buffer size and duration
    REFERENCE_TIME hnsDefaultDuration = 0;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDuration, NULL);
    if (FAILED(hr))
        return NULL;

    hnsDefaultDuration = max(hnsDefaultDuration, 1000000); //100ms minimum

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_EXCLUSIVE,
                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                         hnsDefaultDuration,
                         hnsDefaultDuration,
                         pWaveFormat,
                         NULL);
    
    if (hr ==  AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    {
        UINT bufferSize = 0;
        hr = pAudioClient->GetBufferSize(&bufferSize);
        if (FAILED(hr))
            return NULL;

        const double REFTIME_UNITS_PER_SECOND = 10000000.;
        REFERENCE_TIME hnsAlignedDuration = static_cast<REFERENCE_TIME>(ceil(bufferSize / (pWaveFormat->nSamplesPerSec/ REFTIME_UNITS_PER_SECOND) ));
        r.deactivate();
        SAFE_RELEASE(pAudioClient);
        hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
        if (FAILED(hr) || !pAudioClient)
            return false;
        r.reset(pAudioClient);
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
    r.deactivate();
    return pAudioClient;
}

BOOL FindStreamFormat(IMMDevice * pDevice, int nChannels,int nSampleRate, WAVEFORMATEXTENSIBLE * pwfxt = NULL, IAudioClient * * ppAudioClient = NULL)
{
    if (!pDevice)
         return FALSE;
    
    IAudioClient * pAudioClient = NULL;

   // create a reasonable channel mask
	DWORD dwChannelMask=0;
    DWORD bit=1;
    for (int i=0;i<nChannels;i++)
    {
        dwChannelMask |= bit;
        bit <<= 1;
    }

   WAVEFORMATEXTENSIBLE waveFormat;
    //try 32-bit first
   waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
   waveFormat.Format.nChannels =nChannels;
   waveFormat.Format.nSamplesPerSec = nSampleRate;
   waveFormat.Format.wBitsPerSample = 32;
   waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels/8;
   waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
   waveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
   waveFormat.Samples.wValidBitsPerSample=waveFormat.Format.wBitsPerSample;
   waveFormat.dwChannelMask = dwChannelMask;
   waveFormat.SubFormat =   KSDATAFORMAT_SUBTYPE_PCM;

   pAudioClient = getAudioClient(pDevice,(WAVEFORMATEX*)&waveFormat);
   
   if (pAudioClient)
       goto Finish;

   //try 24-bit containered next
   waveFormat.Samples.wValidBitsPerSample = 24;

   pAudioClient = getAudioClient(pDevice,(WAVEFORMATEX*)&waveFormat);

   if (pAudioClient)
       goto Finish;

   //try 24-bit packed next
   waveFormat.Format.wBitsPerSample = 24;
   waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels/8;
   waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
   waveFormat.Samples.wValidBitsPerSample=waveFormat.Format.wBitsPerSample;

   pAudioClient = getAudioClient(pDevice,(WAVEFORMATEX*)&waveFormat);

   if (pAudioClient)
       goto Finish;

   //finally, try 16-bit   
   waveFormat.Format.wBitsPerSample = 16;
   waveFormat.Format.nBlockAlign = waveFormat.Format.wBitsPerSample * waveFormat.Format.nChannels/8;
   waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
   waveFormat.Samples.wValidBitsPerSample=waveFormat.Format.wBitsPerSample;
   
   pAudioClient = getAudioClient(pDevice,(WAVEFORMATEX*)&waveFormat);

Finish:
   BOOL bSuccess = (pAudioClient!=NULL);
   if (bSuccess)        
   {
      if (pwfxt)
        memcpy_s(pwfxt,sizeof(WAVEFORMATEXTENSIBLE),&waveFormat,sizeof(WAVEFORMATEXTENSIBLE));
      if (ppAudioClient)
        *ppAudioClient = pAudioClient;
      else
        SAFE_RELEASE(pAudioClient)
   }
   return bSuccess;
}

CUnknown* ASIO2WASAPI::CreateInstance (LPUNKNOWN pUnk, HRESULT *phr)
{
	return static_cast<CUnknown*>(new ASIO2WASAPI (pUnk,phr));
};

STDMETHODIMP ASIO2WASAPI::NonDelegatingQueryInterface (REFIID riid, void ** ppv)
{
	if (riid == CLSID_ASIO2WASAPI_DRIVER)
	{
		return GetInterface (this, ppv);
	}
	return CUnknown::NonDelegatingQueryInterface (riid, ppv);
}

ASIOSampleType ASIO2WASAPI::getASIOSampleType() const
{
    switch (m_waveFormat.Format.wBitsPerSample)
    {
        case 16: return  ASIOSTInt16LSB;
        case 24: return  ASIOSTInt24LSB;
        case 32: 
            switch (m_waveFormat.Samples.wValidBitsPerSample)
            {
                case 32: return ASIOSTInt32LSB;
                case 24: return ASIOSTInt32LSB24;
                default: return ASIOSTLastEntry ;
            }
        default: return ASIOSTLastEntry;
    }
}

const char * szChannelRegValName = "Channels";
const char * szSampRateRegValName = "Sample Rate";
const wchar_t * szDeviceId = L"Device Id";

void ASIO2WASAPI::readFromRegistry()
{
    Logger::trace(L"ASIO2WASAPI::readFromRegistery");
    HKEY key = 0;
    LONG lResult = RegOpenKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, KEY_READ,&key);
    if (ERROR_SUCCESS == lResult)
    {
        DWORD size = sizeof (m_nChannels);
        RegGetValue(key,NULL,szChannelRegValName,RRF_RT_REG_DWORD,NULL,&m_nChannels,&size);
        size = sizeof (m_nSampleRate);
        RegGetValue(key,NULL,szSampRateRegValName,RRF_RT_REG_DWORD,NULL,&m_nSampleRate,&size);
        RegGetValueW(key,NULL,szDeviceId,RRF_RT_REG_SZ,NULL,NULL,&size);
        m_deviceId.resize(size/sizeof(m_deviceId[0]));
        if (size) {
            RegGetValueW(key, NULL, szDeviceId, RRF_RT_REG_SZ, NULL, &m_deviceId[0], &size);
        }
        RegCloseKey(key);
        Logger::trace(L" - m_nChannels: %d", m_nChannels);
        Logger::trace(L" - m_nSampleRate: %d", m_nSampleRate);
        Logger::trace(L" - m_deviceId: %ws", &m_deviceId[0]);
    }
}

void ASIO2WASAPI::writeToRegistry()
{
    HKEY key = 0;
    LONG lResult = RegCreateKeyEx(HKEY_CURRENT_USER, szPrefsRegKey, 0, NULL,0, KEY_WRITE,NULL,&key,NULL);
    if (ERROR_SUCCESS == lResult)
    {
        DWORD size = sizeof (m_nChannels);
        RegSetValueEx (key,szChannelRegValName,NULL,REG_DWORD,(const BYTE *)&m_nChannels,size);
        size = sizeof (m_nSampleRate);
        RegSetValueEx(key,szSampRateRegValName,NULL,REG_DWORD,(const BYTE *)&m_nSampleRate,size);
        size = (DWORD)(m_deviceId.size()) * sizeof(m_deviceId[0]);
        RegSetValueExW(key,szDeviceId,NULL,REG_SZ,(const BYTE *)&m_deviceId[0],size);
        RegCloseKey(key);
    }
}

void ASIO2WASAPI::clearState()
{
    //fields valid before initialization
    m_nChannels = 2;
    m_nSampleRate = 48000;
    memset(m_errorMessage,0,sizeof(m_errorMessage));
    m_deviceId.clear();
    m_hStopPlayThreadEvent = NULL; 

    //fields filled by init()/cleaned by shutdown()
    m_active = false;
    m_pDevice = NULL;
    m_pAudioClient = NULL;
    memset(&m_waveFormat,0,sizeof(m_waveFormat));
    m_bufferIndex = 0;
    m_hAppWindowHandle = NULL;

    //fields filled by createBuffers()/cleaned by disposeBuffers()
    m_buffers[0].clear();
    m_buffers[1].clear();
    m_callbacks = NULL;

    //fields filled by start()/cleaned by stop()
    m_hPlayThreadIsRunningEvent = NULL;
    m_bufferSize = 0;
    m_theSystemTime.hi = 0;
    m_theSystemTime.lo = 0;
    m_samplePosition = 0;
}

extern HINSTANCE g_hinstDLL;

ASIO2WASAPI::ASIO2WASAPI (LPUNKNOWN pUnk, HRESULT *phr)
	: CUnknown("ASIO2WASAPI", pUnk, phr)
{
    clearState();
    readFromRegistry();
    openerPtr = std::make_unique<TrayOpener>(
        g_hinstDLL,
        LoadIcon(g_hinstDLL, MAKEINTRESOURCE(IDI_ICON1)),
        [&]() { controlPanel(); },
        TEXT("ASIO2WASAPI2: Open Configuration")
    );
}

ASIO2WASAPI::~ASIO2WASAPI ()
{
    shutdown();
}

void ASIO2WASAPI::shutdown()
{
	stop();
	disposeBuffers();
    SAFE_RELEASE(m_pAudioClient)
    SAFE_RELEASE(m_pDevice)
    clearState();
}

BOOL CALLBACK ASIO2WASAPI::ControlPanelProc(HWND hwndDlg, 
        UINT message, WPARAM wParam, LPARAM lParam)
{ 
    static ASIO2WASAPI * pDriver = NULL;
    static vector< vector<wchar_t> > deviceStringIds;
    switch (message) 
    { 
         case WM_DESTROY:
            pDriver = NULL;
            deviceStringIds.clear();
            return 0;
         case WM_COMMAND: 
            {
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
                    if (pDriver)
                    {
                        int nChannels = 2;
                        int nSampleRate = 48000;
                        //get nChannels and nSampleRate from the dialog
                        {
                            BOOL bSuccess = FALSE;
                            int tmp = (int)GetDlgItemInt(hwndDlg,IDC_CHANNELS,&bSuccess,TRUE);
                            if (bSuccess && tmp >= 0)
                                nChannels = tmp;
                            else {
                                MessageBox(hwndDlg,"Invalid number of channels",szDescription,MB_OK);
                                return 0;                        
                            }
                            tmp = (int)GetDlgItemInt(hwndDlg,IDC_SAMPLE_RATE,&bSuccess,TRUE);
                            if (bSuccess && tmp >= 0)
                                nSampleRate = tmp;
                            else {
                                MessageBox(hwndDlg,"Invalid sample rate",szDescription,MB_OK);
                                return 0;                        
                            }
                        }
                        //get the selected device's index from the dialog
                        LRESULT lr = SendDlgItemMessage(hwndDlg,IDC_DEVICE,CB_GETCURSEL,0,0);
                        if (lr == CB_ERR || lr < 0 || (size_t)lr >= deviceStringIds.size()) {
                            MessageBox(hwndDlg,"No audio device selected",szDescription,MB_OK);
                            return 0;
                        }
                        vector<wchar_t>& selectedDeviceId = deviceStringIds[lr];
                        //find this device
                        IMMDevice * pDevice = NULL;
                        {
                            IMMDeviceEnumerator *pEnumerator = NULL;
                            HRESULT hr = CoCreateInstance(
                                   CLSID_MMDeviceEnumerator, NULL,
                                   CLSCTX_ALL, IID_IMMDeviceEnumerator,
                                   (void**)&pEnumerator);
                            if (FAILED(hr))
                                return 0;
                            CReleaser r1(pEnumerator);

                            IMMDeviceCollection *pMMDeviceCollection = NULL;
                            hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
                            if (FAILED(hr)) 
                                return 0;
                            CReleaser r2(pMMDeviceCollection);
    
                            UINT nDevices = 0;
                            hr = pMMDeviceCollection->GetCount(&nDevices);
                            if (FAILED(hr)) 
                                return 0;
    
                            for (UINT i = 0; i < nDevices; i++) 
                            {
                                IMMDevice *pMMDevice = NULL;
                                hr = pMMDeviceCollection->Item(i, &pMMDevice);
                                if (FAILED(hr)) 
                                    continue ;
                                CReleaser r(pMMDevice);
                                vector<wchar_t> deviceId = getDeviceId(pMMDevice);
                                if (deviceId.size() == 0)
                                    continue;
                                if (wcscmp(&deviceId[0],&selectedDeviceId[0]) == 0)
                                {
                                    pDevice = pMMDevice;
                                    r.deactivate();
                                    break;
                                }
                            }
                        }
                        if (!pDevice)
                        {
                            MessageBox(hwndDlg,"Invalid audio device",szDescription,MB_OK);
                            return 0;
                        }
                        
                        CReleaser r2(pDevice);

                        //make sure the reset request is issued no matter how we proceed
                        class CCallbackCaller
                        {
                            ASIOCallbacks * m_pCallbacks;
                        public:
                            CCallbackCaller(ASIOCallbacks * pCallbacks) : m_pCallbacks(pCallbacks) {}
                            ~CCallbackCaller() 
                            {
                                if (m_pCallbacks)
                                    m_pCallbacks->asioMessage(kAsioResetRequest,0,NULL,NULL);
                            }
                        } caller(pDriver->m_callbacks);
                        
                        //shut down the driver so no exclusive WASAPI connection would stand in our way
                        HWND hAppWindowHandle = pDriver->m_hAppWindowHandle;
                        pDriver->shutdown();

                        //make sure the device supports this combination of nChannels and nSampleRate
                        BOOL rc = FindStreamFormat(pDevice,nChannels,nSampleRate);
                        if (!rc)
                        {
                            MessageBox(hwndDlg,"Sample rate is not supported in WASAPI exclusive mode",szDescription,MB_OK);
                            return 0;
                        }
                        
                        //copy selected device/sample rate/channel combination into the driver
                        pDriver->m_nSampleRate = nSampleRate;
                        pDriver->m_nChannels = nChannels;
                        pDriver->m_deviceId.resize(selectedDeviceId.size());
                        wcscpy_s(&pDriver->m_deviceId[0],selectedDeviceId.size(),&selectedDeviceId.at(0));
                        //try to init the driver
                        if (pDriver->init(hAppWindowHandle) == ASIOFalse)
                        {    
                            MessageBox(hwndDlg,"ASIO driver failed to initialize",szDescription,MB_OK);
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
        case WM_INITDIALOG: 
            {
            pDriver = (ASIO2WASAPI*) lParam;
            if (!pDriver)
                return FALSE;
            SetDlgItemInt(hwndDlg,IDC_CHANNELS,(UINT)pDriver->m_nChannels,TRUE);
            SetDlgItemInt(hwndDlg,IDC_SAMPLE_RATE,(UINT)pDriver->m_nSampleRate,TRUE);

            IMMDeviceEnumerator *pEnumerator = NULL;
            DWORD flags = 0;
            CoInitialize(NULL);

            HRESULT hr = CoCreateInstance(
                   CLSID_MMDeviceEnumerator, NULL,
                   CLSCTX_ALL, IID_IMMDeviceEnumerator,
                   (void**)&pEnumerator);
            if (FAILED(hr))
                return false;
            CReleaser r1(pEnumerator);

            IMMDeviceCollection *pMMDeviceCollection = NULL;
            hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
            if (FAILED(hr)) 
                return false;
            CReleaser r2(pMMDeviceCollection);
    
            UINT nDevices = 0;
            hr = pMMDeviceCollection->GetCount(&nDevices);
            if (FAILED(hr)) 
                return false;
    
            vector< vector<wchar_t> > deviceIds;
            for (UINT i = 0; i < nDevices; i++) 
            {
                IMMDevice *pMMDevice = NULL;
                hr = pMMDeviceCollection->Item(i, &pMMDevice);
                if (FAILED(hr)) 
                    return false;
                CReleaser r(pMMDevice);

                vector<wchar_t> deviceId = getDeviceId(pMMDevice);
                if (deviceId.size() == 0)
                    return false;
                deviceIds.push_back(deviceId);

                IPropertyStore *pPropertyStore;
                hr = pMMDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
                if (FAILED(hr)) 
                    return false;
                CReleaser r2(pPropertyStore);
                PROPVARIANT var; PropVariantInit(&var);
                hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName,&var);
                if (FAILED(hr))
                    return false;
                LRESULT lr = 0;
                if (var.vt != VT_LPWSTR ||
                    (lr = SendDlgItemMessageW(hwndDlg,IDC_DEVICE,CB_ADDSTRING,0,(LPARAM)var.pwszVal)) == CB_ERR)
                {
                    PropVariantClear(&var);
                    return false;
                }
                PropVariantClear(&var);
            }
            deviceStringIds = deviceIds;

            //find current device id
            int nDeviceIdIndex = -1;
            if (pDriver->m_deviceId.size())
                for (unsigned i=0;i<deviceStringIds.size(); i++)
                {
                    if (wcscmp(&deviceStringIds[i].at(0),&pDriver->m_deviceId[0]) == 0)
                    {    
                        nDeviceIdIndex = i;
                        break;
                    }
                }
            SendDlgItemMessage(hwndDlg,IDC_DEVICE,CB_SETCURSEL,nDeviceIdIndex,0);
            return TRUE;
            }
            break;
    } 
    return FALSE; 
} 

#define RETURN_ON_ERROR(hres)  \
              if (FAILED(hres)) return -1;


DWORD WINAPI ASIO2WASAPI::PlayThreadProc(LPVOID pThis) 
{
    ASIO2WASAPI * pDriver = static_cast<ASIO2WASAPI *>(pThis);
    struct CExitEventSetter
    {
        HANDLE & m_hEvent;
        CExitEventSetter(ASIO2WASAPI * pDriver):m_hEvent(pDriver->m_hPlayThreadIsRunningEvent)
        {
            m_hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
        }
        ~CExitEventSetter()
        {
            SetEvent(m_hEvent);
            CloseHandle(m_hEvent);
            m_hEvent = NULL;
        }
    } setter(pDriver);

    HRESULT hr=S_OK;

    IAudioClient *pAudioClient = pDriver->m_pAudioClient;
    IAudioRenderClient *pRenderClient = NULL;
    BYTE *pData = NULL;
                                              
    hr = CoInitialize(NULL);
    RETURN_ON_ERROR(hr)

    // Create an event handle and register it for
    // buffer-event notifications.
    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    CHandleCloser cl(hEvent);

    hr = pAudioClient->SetEventHandle(hEvent);
    RETURN_ON_ERROR(hr)

    hr = pAudioClient->GetService(
                         IID_IAudioRenderClient,
                         (void**)&pRenderClient);

    RETURN_ON_ERROR(hr)
    CReleaser r(pRenderClient);
    
    // Ask MMCSS to temporarily boost the thread priority
    // to reduce the possibility of glitches while we play.
    DWORD taskIndex = 0;
    AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);

    // Pre-load the first buffer with data
    // from the audio source before starting the stream.
    hr = pDriver->LoadData(pRenderClient);
    RETURN_ON_ERROR(hr)

    hr = pAudioClient->Start();  // Start playing.
    RETURN_ON_ERROR(hr)

    getNanoSeconds(&pDriver->m_theSystemTime);
    pDriver->m_samplePosition = 0;    
    
    if (pDriver->m_callbacks)
        pDriver->m_callbacks->bufferSwitch(1-pDriver->m_bufferIndex,ASIOTrue);
    
    DWORD retval = 0;
    HANDLE events[2] = {pDriver->m_hStopPlayThreadEvent, hEvent };
    while ((retval  = WaitForMultipleObjects(2,events,FALSE, INFINITE)) == (WAIT_OBJECT_0 + 1))
    {//the hEvent is signalled and m_hStopPlayThreadEvent is not
        // Grab the next empty buffer from the audio device.
        hr = pDriver->LoadData(pRenderClient);
        getNanoSeconds(&pDriver->m_theSystemTime);
        pDriver->m_samplePosition += pDriver->m_bufferSize;
        if (pDriver->m_callbacks)
            pDriver->m_callbacks->bufferSwitch(1-pDriver->m_bufferIndex,ASIOTrue);
    }

    hr = pAudioClient->Stop();  // Stop playing.
    RETURN_ON_ERROR(hr)
    pDriver->m_samplePosition = 0;    

    return 0;
}

#undef RETURN_ON_ERROR

HRESULT ASIO2WASAPI::LoadData(IAudioRenderClient * pRenderClient)
{
    if (!pRenderClient)
        return E_INVALIDARG;
    
    HRESULT hr = S_OK;
    BYTE *pData = NULL;
    hr = pRenderClient->GetBuffer(m_bufferSize, &pData);

    UINT32 sampleSize=m_waveFormat.Format.wBitsPerSample/8;
    
    //switch buffer
    m_bufferIndex = 1 - m_bufferIndex;
    vector <vector <BYTE> > &buffer = m_buffers[m_bufferIndex]; 
    unsigned sampleOffset = 0;
    unsigned nextSampleOffset = sampleSize;
    for (int i = 0;i < m_bufferSize; i++, sampleOffset=nextSampleOffset, nextSampleOffset+=sampleSize)
    {
        for (unsigned j = 0; j < buffer.size(); j++) 
        {
            if (buffer[j].size() >= nextSampleOffset)
                memcpy_s(pData,sampleSize,&buffer[j].at(0)+sampleOffset,sampleSize);
            else
                memset(pData,0,sampleSize);
            pData+=sampleSize;
        }
    }

    hr = pRenderClient->ReleaseBuffer(m_bufferSize, 0);

    return S_OK;
}

/*  ASIO driver interface implementation
*/

void ASIO2WASAPI::getDriverName (char *name)
{
	strcpy_s (name, 32, "ASIO2WASAPI");
}

long ASIO2WASAPI::getDriverVersion ()
{
	return 1;
}

void ASIO2WASAPI::getErrorMessage (char *string)
{
    strcpy_s(string,sizeof(m_errorMessage),m_errorMessage);
}

ASIOError ASIO2WASAPI::future (long selector, void* opt)	
{
    //none of the optional features are present
    return ASE_NotPresent;
}

ASIOError ASIO2WASAPI::outputReady ()
{
    //No latency reduction can be achieved, return ASE_NotPresent
    return ASE_NotPresent;
}

ASIOError ASIO2WASAPI::getChannels (long *numInputChannels, long *numOutputChannels)
{
    if (!m_active)
        return ASE_NotPresent;

    if (numInputChannels)
        *numInputChannels = 0;
	if (numOutputChannels)
        *numOutputChannels = m_nChannels;
	return ASE_OK;
}

ASIOError ASIO2WASAPI::controlPanel()
{
    extern HINSTANCE g_hinstDLL;
    DialogBoxParam(g_hinstDLL,MAKEINTRESOURCE(IDD_CONTROL_PANEL),m_hAppWindowHandle,(DLGPROC)ControlPanelProc,(LPARAM)this);
    return ASE_OK;
}

void ASIO2WASAPI::setMostReliableFormat()
{
    m_nChannels = 2;
    m_nSampleRate = 48000;
    memset(&m_waveFormat,0,sizeof(m_waveFormat));
    WAVEFORMATEX& fmt = m_waveFormat.Format;
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = 48000;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    fmt.wBitsPerSample = 16;
}

ASIOBool ASIO2WASAPI::init(void* sysRef)
{
	if (m_active)
		return true;

    m_hAppWindowHandle = (HWND) sysRef;

    HRESULT hr=S_OK;
    IMMDeviceEnumerator *pEnumerator = NULL;
    DWORD flags = 0;

    CoInitialize(NULL);

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    if (FAILED(hr))
        return false;
    CReleaser r1(pEnumerator);

    IMMDeviceCollection *pMMDeviceCollection = NULL;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
    if (FAILED(hr)) 
        return false;
    CReleaser r2(pMMDeviceCollection);
    
    UINT nDevices = 0;
    hr = pMMDeviceCollection->GetCount(&nDevices);
    if (FAILED(hr)) 
        return false;
    
    bool bDeviceFound = false;
    for (UINT i = 0; i < nDevices; i++) 
    {
        IMMDevice *pMMDevice = NULL;
        hr = pMMDeviceCollection->Item(i, &pMMDevice);
        if (FAILED(hr)) 
            return false;
        CReleaser r(pMMDevice);

        vector<wchar_t> deviceId = getDeviceId(pMMDevice);
        if (deviceId.size() && m_deviceId.size() && wcscmp(&deviceId[0],&m_deviceId[0]) == 0)
        {
            m_pDevice = pMMDevice;
            m_pDevice->AddRef();
            bDeviceFound = true;
            break;
        }
    }
    
    if (!bDeviceFound)
    {//id not found 
        hr = pEnumerator->GetDefaultAudioEndpoint(
                            eRender, eConsole, &m_pDevice);
        if (FAILED(hr))
            return false;
        setMostReliableFormat();
    }
    
    m_deviceId = getDeviceId(m_pDevice);

    BOOL rc = FindStreamFormat(m_pDevice, m_nChannels,m_nSampleRate,&m_waveFormat,&m_pAudioClient);
    if (!rc)
    {//go through all devices and try to find the one that works for 16/48K
        SAFE_RELEASE(m_pDevice)
        setMostReliableFormat();
        
        IMMDeviceCollection *pMMDeviceCollection = NULL;
        hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection);
        if (FAILED(hr)) 
            return false;
        CReleaser r2(pMMDeviceCollection);
    
        UINT nDevices = 0;
        hr = pMMDeviceCollection->GetCount(&nDevices);
        if (FAILED(hr)) 
            return false;
    
        for (UINT i = 0; i < nDevices; i++) 
        {
            IMMDevice *pMMDevice = NULL;
            hr = pMMDeviceCollection->Item(i, &pMMDevice);
            if (FAILED(hr)) 
                continue;
            CReleaser r(pMMDevice);
            rc = FindStreamFormat(pMMDevice, m_nChannels,m_nSampleRate,&m_waveFormat,&m_pAudioClient);
            if (rc)
            {
                m_pDevice = pMMDevice;
                r.deactivate();
                break;
            }
        }
        if (!m_pAudioClient)
            return false; //suitable device not found
    }

    UINT32 bufferSize = 0;
    hr = m_pAudioClient->GetBufferSize(&bufferSize);
    if (FAILED(hr))
        return false;

    m_bufferSize = bufferSize;
    m_active = true;
    
    return true;
}

ASIOError ASIO2WASAPI::getSampleRate (ASIOSampleRate *sampleRate)
{
    if (!sampleRate)
        return ASE_InvalidParameter;
    if (!m_active)
        return ASE_NotPresent;
    *sampleRate = m_nSampleRate;
    
    return ASE_OK;
}

ASIOError ASIO2WASAPI::setSampleRate (ASIOSampleRate sampleRate)
{
    if (!m_active)
        return ASE_NotPresent;

    if (sampleRate == m_nSampleRate)
        return ASE_OK;

    ASIOError err = canSampleRate(sampleRate);
    if (err != ASE_OK)
        return err;
    
    int nPrevSampleRate = m_nSampleRate;
    m_nSampleRate = (int)sampleRate;
    writeToRegistry();
    if (m_callbacks)
    {//ask the host ro reset us
        m_nSampleRate = nPrevSampleRate;
        m_callbacks->asioMessage(kAsioResetRequest,0,NULL,NULL);
    }
    else
    {//reinitialize us with the new sample rate
        HWND hAppWindowHandle = m_hAppWindowHandle;
        shutdown();
        readFromRegistry();
        init(hAppWindowHandle);
    }
    
    return ASE_OK;
}

//all buffer sizes are in frames
ASIOError ASIO2WASAPI::getBufferSize (long *minSize, long *maxSize,
	long *preferredSize, long *granularity)
{
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

ASIOError ASIO2WASAPI::createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,
	long bufferSize, ASIOCallbacks *callbacks)
{
    if (!m_active)
        return ASE_NotPresent;

    //some sanity checks
    if (!callbacks || numChannels < 0 || numChannels > m_nChannels)
        return ASE_InvalidParameter;
    if (bufferSize != m_bufferSize)
        return ASE_InvalidMode;
    for (int i = 0; i < numChannels; i++)
	{
        ASIOBufferInfo &info = bufferInfos[i];
        if (info.isInput || info.channelNum < 0 || info.channelNum >= m_nChannels)
            return ASE_InvalidMode;
    }
    
    //dispose exiting buffers
    disposeBuffers();

    m_callbacks = callbacks;
    int sampleContainerLength = m_waveFormat.Format.wBitsPerSample/8;
    int bufferByteLength=bufferSize*sampleContainerLength;

    //the very allocation
    m_buffers[0].resize(m_nChannels);
    m_buffers[1].resize(m_nChannels);
    
    for (int i = 0; i < numChannels; i++)
	{
        ASIOBufferInfo &info = bufferInfos[i];
        m_buffers[0].at(info.channelNum).resize(bufferByteLength);
        m_buffers[1].at(info.channelNum).resize(bufferByteLength);
        info.buffers[0] = &m_buffers[0].at(info.channelNum)[0];
        info.buffers[1] = &m_buffers[1].at(info.channelNum)[0];
    }		
    return ASE_OK;
}

ASIOError ASIO2WASAPI::disposeBuffers()
{
	stop();
	//wait for the play thread to finish
    WaitForSingleObject(m_hPlayThreadIsRunningEvent,INFINITE);
    m_callbacks = 0;
    m_buffers[0].clear();
    m_buffers[1].clear();
    
    return ASE_OK;
}

ASIOError ASIO2WASAPI::getChannelInfo (ASIOChannelInfo *info)
{
    if (!m_active)
        return ASE_NotPresent;

    if (info->channel < 0 || info->channel >=m_nChannels ||  info->isInput)
		return ASE_InvalidParameter;

    info->type = getASIOSampleType();
    info->channelGroup = 0;
    info->isActive = (m_buffers[0].size() > 0) ? ASIOTrue:ASIOFalse;
    const char * knownChannelNames[] = 
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
        (info->channel < sizeof(knownChannelNames)/sizeof(knownChannelNames[0])) ? knownChannelNames[info->channel] : "Unknown");

    return ASE_OK;
}

ASIOError ASIO2WASAPI::canSampleRate (ASIOSampleRate sampleRate)
{
    if (!m_active)
        return ASE_NotPresent;

	int nSampleRate = static_cast<int>(sampleRate);
    return FindStreamFormat(m_pDevice,m_nChannels,nSampleRate) ? ASE_OK : ASE_NoClock;
}

ASIOError ASIO2WASAPI::start()
{
    if (!m_active || !m_callbacks)
        return ASE_NotPresent;
    if (m_hStopPlayThreadEvent)
        return ASE_OK;// we are already playing
    //make sure the previous play thread exited
    WaitForSingleObject(m_hPlayThreadIsRunningEvent,INFINITE);
    
    m_hStopPlayThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    CreateThread(NULL,0,PlayThreadProc,this,0,NULL);

    return ASE_OK;
}

ASIOError ASIO2WASAPI::stop()
{
    if (!m_active)
        return ASE_NotPresent;
    if (!m_hStopPlayThreadEvent)
        return ASE_OK; //we already stopped

    //set the thead stopping event, thus initiating the thread termination process
    SetEvent(m_hStopPlayThreadEvent);
    CloseHandle(m_hStopPlayThreadEvent);
    m_hStopPlayThreadEvent = NULL;

    return ASE_OK;
}

ASIOError ASIO2WASAPI::getClockSources (ASIOClockSource *clocks, long *numSources)
{
    if (!numSources || *numSources == 0)
        return ASE_OK;
    clocks->index = 0;
    clocks->associatedChannel = -1;
    clocks->associatedGroup = -1;
    clocks->isCurrentSource = ASIOTrue;
    strcpy_s(clocks->name,"Internal clock");
    *numSources = 1;
    return ASE_OK;
}

ASIOError ASIO2WASAPI::setClockSource (long index)
{
    return (index == 0) ? ASE_OK : ASE_NotPresent;
}

ASIOError ASIO2WASAPI::getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
    if (!m_active || !m_callbacks)
        return ASE_NotPresent;
	if (tStamp)
    {
        tStamp->lo = m_theSystemTime.lo;
	    tStamp->hi = m_theSystemTime.hi;
    }
	if (sPos)
    {
        if (m_samplePosition >= twoRaisedTo32)
	    {
		    sPos->hi = (unsigned long)(m_samplePosition * twoRaisedTo32Reciprocal);
		    sPos->lo = (unsigned long)(m_samplePosition - (sPos->hi * twoRaisedTo32));
	    }
	    else
	    {
		    sPos->hi = 0;
		    sPos->lo = (unsigned long)m_samplePosition;
	    }
    }
    return ASE_OK;
}

ASIOError ASIO2WASAPI::getLatencies (long *_inputLatency, long *_outputLatency)
{
    if (!m_active || !m_callbacks)
        return ASE_NotPresent;
    if (_inputLatency)
        *_inputLatency = m_bufferSize;
    if (_outputLatency)
        *_outputLatency = 2 * m_bufferSize;
    return ASE_OK;
}


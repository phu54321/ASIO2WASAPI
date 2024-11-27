// Copyright (C) 2024 Hyunwoo Park
//
// This file is part of trgkASIO.
//
// trgkASIO is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// trgkASIO is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//


#include <functional>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include "IPolicyConfig.h"
#include "WASAPIUtils.h"
#include "raiiUtils.h"

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);


bool iterateAudioEndPoints(const std::function<bool(IMMDevicePtr pMMDevice)> &cb) {
    IMMDeviceEnumerator *pEnumerator_ = nullptr;
    HRESULT hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator,
            (void **) &pEnumerator_);
    if (FAILED(hr))
        return false;

    auto pEnumerator = make_autorelease(pEnumerator_);

    IMMDeviceCollection *pMMDeviceCollection_ = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pMMDeviceCollection_);
    if (FAILED(hr))
        return false;
    auto pMMDeviceCollection = make_autorelease(pMMDeviceCollection_);

    UINT nDevices = 0;
    hr = pMMDeviceCollection->GetCount(&nDevices);
    if (FAILED(hr))
        return false;

    for (UINT i = 0; i < nDevices; i++) {
        IMMDevice *pMMDevice_ = nullptr;
        hr = pMMDeviceCollection->Item(i, &pMMDevice_);
        if (FAILED(hr))
            return false;
        auto pMMDevice = make_autorelease(pMMDevice_);

        if (!cb(pMMDevice))
            break;
    }
    return true;
}


IMMDevicePtr getDefaultOutputDevice() {
    IMMDeviceEnumerator *pEnumerator_ = nullptr;

    HRESULT hr = CoCreateInstance(
            CLSID_MMDeviceEnumerator, nullptr,
            CLSCTX_ALL, IID_IMMDeviceEnumerator,
            (void **) &pEnumerator_);
    if (FAILED(hr)) return nullptr;
    auto pEnumerator = make_autorelease(pEnumerator_);

    IMMDevice *device;
    hr = pEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &device);
    if (FAILED(hr)) return nullptr;

    return make_autorelease(device);
}

bool setDefaultOutputDeviceId(const std::wstring &deviceId) {
    IPolicyConfigVista *pPolicyConfig;
    ERole reserved = eConsole;

    HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient),
                                  NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID *) &pPolicyConfig);
    if (SUCCEEDED(hr)) {
        hr = pPolicyConfig->SetDefaultEndpoint(deviceId.c_str(), reserved);
        pPolicyConfig->Release();
    }
    return SUCCEEDED(hr);
}

////

std::vector<IMMDevicePtr> getIMMDeviceList() {
    std::vector<IMMDevicePtr> v;
    iterateAudioEndPoints([&](auto p) {
        v.push_back(p);
        return true;
    });
    return v;
}

std::wstring getDeviceId(const IMMDevicePtr &pDevice) {
    std::wstring id;
    LPWSTR pDeviceId = nullptr;
    HRESULT hr = pDevice->GetId(&pDeviceId);
    if (FAILED(hr))
        return id;
    id = pDeviceId;
    CoTaskMemFree(pDeviceId);
    pDeviceId = nullptr;
    return id;
}

std::wstring getDeviceFriendlyName(const IMMDevicePtr &pDevice) {
    IPropertyStore *pPropertyStore_;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore_);
    if (FAILED(hr)) return L"";
    auto pPropertyStore = make_autorelease(pPropertyStore_);

    PROPVARIANT var;
    PropVariantInit(&var);
    hr = pPropertyStore->GetValue(PKEY_Device_FriendlyName, &var);
    if (FAILED(hr) || var.vt != VT_LPWSTR) {
        PropVariantClear(&var);
        return L"";
    }
    std::wstring ret = var.pwszVal;
    PropVariantClear(&var);
    return ret;
}

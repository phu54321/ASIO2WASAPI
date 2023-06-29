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

#include "ASIO2WASAPI2.h"
#include "../utils/logger.h"
#include "../resource.h"
#include "../WASAPIOutput/createIAudioClient.h"
#include "../utils/WASAPIUtils.h"

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

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

#include <thread>

#include "../utils/dpiRaii.h"
#include "../utils/logger.h"

#include "./UserPref.h"
#include "../res/resource.h"
#include <spdlog/fmt/fmt.h>
#include <windowsx.h>
#include <CommCtrl.h>

INT_PTR UserPrefEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto pref = loadUserPref();
            SetDlgItemInt(hWnd, IDC_CHANNEL_COUNT, pref->channelCount, FALSE);
            SetDlgItemInt(hWnd, IDC_CLAP_GAIN, (int) round(pref->clapGain * 100), FALSE);

            auto hThrottleCheckbox = GetDlgItem(hWnd, IDC_THROTTLE);
            Button_SetCheck(hThrottleCheckbox, pref->throttle ? BST_CHECKED : BST_UNCHECKED);

            auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
            for (const auto &s: pref->deviceIdList) {
                SendMessageW(hOutputDeviceList, LB_ADDSTRING, 0, (LPARAM) s.c_str());
            }
            return TRUE;
        }

        case WM_CLOSE: {
            DestroyWindow(hWnd);
            return TRUE;
        }

        case WM_DESTROY:
            // TODO: remove postquitmessage.
            PostQuitMessage(0);
            return TRUE;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDOK: {
                    bool ok = false;
                    auto pref = loadUserPref();
                    do {
                        BOOL success;
                        auto channelCount = GetDlgItemInt(hWnd, IDC_CHANNEL_COUNT, &success, FALSE);
                        if (!success) break;
                        pref->channelCount = channelCount;

                        auto clapGain = GetDlgItemInt(hWnd, IDC_CLAP_GAIN, &success, FALSE) / 100.0;
                        if (!success) break;
                        if (clapGain < 0) clapGain = 0;
                        if (clapGain > 1) clapGain = 1;
                        pref->clapGain = clapGain;

                        auto hThrottleCheckbox = GetDlgItem(hWnd, IDC_THROTTLE);
                        auto checked = Button_GetCheck(hThrottleCheckbox);
                        pref->throttle = (checked == BST_CHECKED);

                        pref->deviceIdList.clear();
                        auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                        int lbCount = SendMessage(hOutputDeviceList, LB_GETCOUNT, 0, 0);
                        for (int i = 0; i < lbCount; i++) {
                            auto sLen = SendMessageW(hOutputDeviceList, LB_GETTEXTLEN, i, 0);
                            std::vector<wchar_t> buf(sLen + 1);
                            SendMessageW(hOutputDeviceList, LB_GETTEXT, i, (LPARAM) buf.data());
                            buf[sLen] = 0;
                            pref->deviceIdList.push_back(buf.data());
                        }

                        ok = true;
                        break;
                    } while (false);

                    if (ok) {
                        saveUserPref(pref, TEXT("test.json"));
                        DestroyWindow(hWnd);
                    } else {
                        MessageBox(hWnd, TEXT("Error parsing preference"), TEXT("ERROR"), MB_OK);
                    }

                    break;
                }
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}


void driverSettingsGUIThread() {
    auto hInstance = GetModuleHandle(nullptr);
    auto hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_USERPREF_EDIT), nullptr, UserPrefEditWndProc);

    MSG msg;
    BOOL bret;
    while ((bret = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        if (bret == -1) {
            mainlog->error("GetMessage returned 0x{:08X}", bret);
            break;
        }
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

#ifdef ASIO2WASAPI_PREFGUI_TEST_MAIN

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR lpCmdLine, int nCmdShow) {
    initMainLog();

    std::thread t(driverSettingsGUIThread);
    t.join();
    return 0;
}

#endif

// Copyright (C) 2023 Hyunwoo Park
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
// along with trgkASIO.  If not, see <http://www.gnu.org/licenses/>.
//

#include <thread>

#include "UserPref.h"

#include "../utils/dpiRaii.h"
#include "../utils/logger.h"
#include "../utils/w32StringGetter.h"

#include "DlgOutputDevice.h"
#include "DlgDurationOverrideEditor.h"

#include "../res/resource.h"
#include "../MessageWindow/MessageWindow.h"

#include <windowsx.h>

static std::vector<std::pair<spdlog::level::level_enum, LPCTSTR >> g_errorLevels = {
        {spdlog::level::err,   TEXT("error")},
        {spdlog::level::warn,  TEXT("warning")},
        {spdlog::level::info,  TEXT("info")},
        {spdlog::level::debug, TEXT("debug (VERBOSE)")},
        {spdlog::level::trace, TEXT("trace (VERY VERBOSE)")},
};




INT_PTR CALLBACK DlgUserPrefEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto hInstance = (HINSTANCE) GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

    switch (uMsg) {
        case WM_INITDIALOG: {
            auto pref = loadUserPref();
            // Convert to raw pointer to be stored on GWLP_USERDATA
            auto prefPtr = new UserPrefPtr(pref);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) prefPtr);

            SetDlgItemInt(hWnd, IDC_CHANNEL_COUNT, pref->channelCount, FALSE);
            SetDlgItemInt(hWnd, IDC_CLAP_GAIN, (int) round(pref->clapGain * 100), FALSE);

            auto hThrottleCheckbox = GetDlgItem(hWnd, IDC_THROTTLE);
            Button_SetCheck(hThrottleCheckbox, pref->throttle ? BST_CHECKED : BST_UNCHECKED);

            auto hLogLevel = GetDlgItem(hWnd, IDC_LOGLEVEL);
            for (int i = 0; i < g_errorLevels.size(); i++) {
                const auto &p = g_errorLevels[i];
                ComboBox_AddString(hLogLevel, p.second);
                if (pref->logLevel == p.first) {
                    ComboBox_SetCurSel(hLogLevel, i);
                }
            }

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

        case WM_DESTROY: {
            auto prefPtr = reinterpret_cast<UserPrefPtr *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            delete prefPtr;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);

            auto parentWindow = (HWND) GetWindowLongPtr(hWnd, GWLP_HWNDPARENT);
            if (parentWindow) {
                SendMessage(parentWindow, WM_USER_REMOVEDLG, 0, (LPARAM) hWnd);
            }
            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDOK: {
                    bool ok = false;
                    auto pref = *reinterpret_cast<UserPrefPtr *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
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

                        auto hLogLevel = GetDlgItem(hWnd, IDC_LOGLEVEL);
                        auto errorLevelSel = ComboBox_GetCurSel(hLogLevel);
                        if (errorLevelSel != CB_ERR) {
                            pref->logLevel = g_errorLevels[errorLevelSel].first;
                        }

                        pref->deviceIdList.clear();
                        auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                        int lbCount = SendMessage(hOutputDeviceList, LB_GETCOUNT, 0, 0);
                        for (int i = 0; i < lbCount; i++) {
                            pref->deviceIdList.push_back(ListBox_GetWString(hOutputDeviceList, i));
                        }

                        ok = true;
                    } while (false);

                    if (ok) {
                        saveUserPref(pref);
                        DestroyWindow(hWnd);
                    } else {
                        MessageBox(hWnd, TEXT("Error parsing preference"), TEXT("ERROR"), MB_OK);
                    }

                    break;
                }

                case IDCANCEL:
                    DestroyWindow(hWnd);
                    break;

                case IDB_ADD: {
                    std::wstring deviceName;
                    if (PromptDeviceName(hInstance, hWnd, &deviceName)) {
                        auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                        SendMessageW(hOutputDeviceList, LB_ADDSTRING, 0, (LPARAM) deviceName.c_str());
                    }
                    break;
                }

                case IDB_REMOVE: {
                    auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                    auto curSel = ListBox_GetCurSel(hOutputDeviceList);
                    if (curSel != LB_ERR) {
                        SendMessageW(hOutputDeviceList, LB_DELETESTRING, curSel, 0);
                    }
                    break;
                }

                case IDB_MOVE_UP: {
                    auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                    auto curSel = ListBox_GetCurSel(hOutputDeviceList);
                    if (curSel != LB_ERR && curSel != 0) {
                        auto content = ListBox_GetWString(hOutputDeviceList, curSel);
                        SendMessageW(hOutputDeviceList, LB_DELETESTRING, curSel, 0);
                        SendMessageW(hOutputDeviceList, LB_INSERTSTRING, curSel - 1, (LPARAM) content.c_str());
                        ListBox_SetCurSel(hOutputDeviceList, curSel - 1);
                    }
                    break;
                }

                case IDB_MOVE_DOWN: {
                    auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                    auto curSel = ListBox_GetCurSel(hOutputDeviceList);
                    auto listLen = ListBox_GetCount(hOutputDeviceList);
                    if (curSel != LB_ERR && curSel != listLen - 1) {
                        auto content = ListBox_GetWString(hOutputDeviceList, curSel);
                        SendMessageW(hOutputDeviceList, LB_DELETESTRING, curSel, 0);
                        SendMessageW(hOutputDeviceList, LB_INSERTSTRING, curSel + 1, (LPARAM) content.c_str());
                        ListBox_SetCurSel(hOutputDeviceList, curSel + 1);
                    }
                    break;
                }

                case IDC_OUTPUT_DEVICE_LIST: {
                    auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                    switch (HIWORD(wParam)) {
                        case LBN_DBLCLK: {
                            auto curSel = ListBox_GetCurSel(hOutputDeviceList);
                            if (curSel != LB_ERR) {
                                auto deviceName = ListBox_GetWString(hOutputDeviceList, curSel);
                                if (PromptDeviceName(hInstance, hWnd, &deviceName)) {
                                    SendMessageW(hOutputDeviceList, LB_DELETESTRING, curSel, 0);
                                    SendMessageW(hOutputDeviceList, LB_INSERTSTRING, curSel,
                                                 (LPARAM) deviceName.c_str());
                                }
                            }
                        }
                    }
                    break;
                }

                case IDB_OUTPUT_LATENCY_OVERRIDE: {
                    auto prefPtr = reinterpret_cast<UserPrefPtr *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                    DialogDurationOverrideEditor(hInstance, hWnd, *prefPtr);
                }
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}


HWND createUserPrefEditDialog(HINSTANCE hInstance, HWND hwndParent = nullptr) {
    return CreateDialog(hInstance, MAKEINTRESOURCE(IDD_USERPREF_EDIT), hwndParent, DlgUserPrefEditWndProc);
}

#ifdef TRGKASIO_PREFGUI_MAIN

#include <objbase.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PSTR lpCmdLine, int nCmdShow) {
    CoInitialize(nullptr);
    initMainLog();

    auto hDlg = createUserPrefEditDialog(hInstance, nullptr);

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
        if(hDlg && !IsWindow(hDlg)) {
            PostQuitMessage(0);
            hDlg = nullptr;
        }
    }

    return 0;
}

#endif

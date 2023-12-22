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
#include "../utils/WASAPIUtils.h"
#include "../utils/dlgGetText.h"

#include "./UserPref.h"
#include "../res/resource.h"
#include <spdlog/fmt/fmt.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <sstream>
#include <regex>

static std::vector<std::pair<spdlog::level::level_enum, LPCTSTR >> errorLevels = {
        {spdlog::level::err,   TEXT("error")},
        {spdlog::level::warn,  TEXT("warning")},
        {spdlog::level::info,  TEXT("info")},
        {spdlog::level::debug, TEXT("debug")},
        {spdlog::level::trace, TEXT("trace")},
};

LPCWSTR listSeparator = L"----------------";

INT_PTR DlgUserPrefOutputDeviceProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto deviceNameBuffer = reinterpret_cast<std::wstring *>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) deviceNameBuffer);

            auto devices = getIMMDeviceList();
            auto hDeviceList = GetDlgItem(hWnd, IDC_DEVICE);
            std::vector<std::wstring> candidates;

            candidates.emplace_back(L"(default)");
            candidates.emplace_back(listSeparator);

            for (const auto &d: devices) {
                auto friendlyName = getDeviceFriendlyName(d);
                candidates.emplace_back(friendlyName);
            }

            candidates.emplace_back(listSeparator);
            for (const auto &d: devices) {
                auto deviceId = getDeviceId(d);
                candidates.emplace_back(deviceId);
            }

            for (const auto &c: candidates) {
                SendMessageW(hDeviceList, CB_ADDSTRING, 0, (LPARAM) c.c_str());
            }

            SetWindowTextW(hDeviceList, deviceNameBuffer->c_str());

            return TRUE;
        }

        case WM_CLOSE:
            EndDialog(hWnd, FALSE);
            return TRUE;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDCANCEL:
                    EndDialog(hWnd, FALSE);
                    return TRUE;

                case IDOK: {
                    auto deviceNameBuffer = reinterpret_cast<std::wstring *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

                    auto hDeviceList = GetDlgItem(hWnd, IDC_DEVICE);
                    auto deviceSelected = getWndText(hDeviceList);

                    if (!deviceSelected.empty() && deviceSelected != listSeparator) {
                        *deviceNameBuffer = deviceSelected;
                        EndDialog(hWnd, TRUE);
                    } else {
                        EndDialog(hWnd, FALSE);
                    }
                    return TRUE;
                }
                default:
                    return FALSE;
            }
        }


        default:
            return FALSE;
    }
}

std::wstring trim(std::wstring str) {
    str.erase(str.find_last_not_of(' ') + 1);         //suffixing spaces
    str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
    return str;
}

INT_PTR DlgUserPrefDurationOverrideEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto prefPtr = reinterpret_cast<UserPrefPtr *>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) prefPtr);

            std::wstringstream wss;
            auto &pref = *prefPtr;
            for (const auto &it: pref->durationOverride) {
                wss << it.first << L": " << it.second << L"\r\n";
            }

            auto hTextEdit = GetDlgItem(hWnd, IDC_TEXTEDIT);
            SetWindowTextW(hTextEdit, wss.str().c_str());

            return TRUE;
        }

        case WM_CLOSE:
            EndDialog(hWnd, FALSE);
            return TRUE;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDCANCEL:
                    EndDialog(hWnd, FALSE);
                    return TRUE;

                case IDOK: {
                    auto prefPtr = reinterpret_cast<UserPrefPtr *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                    auto &pref = *prefPtr;
                    auto hTextEdit = GetDlgItem(hWnd, IDC_TEXTEDIT);
                    auto text = getWndText(hTextEdit);
                    text = std::regex_replace(text, std::wregex(L"\r"), L"");

                    std::map<std::wstring, int> table;
                    std::wregex r(L"^(.+):\\s*(\\d+)$");

                    auto itBegin = std::wsregex_iterator(text.begin(), text.end(), r);
                    auto itEnd = std::wsregex_iterator();
                    for (auto i = itBegin; i != itEnd; ++i) {
                        const auto &sm = *i;
                        auto deviceId = trim(sm[1].str());
                        auto duration = std::stoi(sm[2].str());
                        table[deviceId] = duration;
                    }

                    pref->durationOverride = table;
                    EndDialog(hWnd, TRUE);
                    return TRUE;
                }
                default:
                    return FALSE;
            }
        }


        default:
            return FALSE;
    }
}

INT_PTR DlgUserPrefEditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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
            for (int i = 0; i < errorLevels.size(); i++) {
                const auto &p = errorLevels[i];
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

            // TODO: remove postquitmessage.
            PostQuitMessage(0);
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
                            pref->logLevel = errorLevels[errorLevelSel].first;
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
                        saveUserPref(pref, TEXT("test.json"));
                        DestroyWindow(hWnd);
                    } else {
                        MessageBox(hWnd, TEXT("Error parsing preference"), TEXT("ERROR"), MB_OK);
                    }

                    break;
                }

                case IDCANCEL:
                    DestroyWindow(hWnd);
                    break;

                case IDB_DEVICELIST_ADD: {
                    std::wstring deviceName;
                    if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_USERPREF_OUTPUT_DEVICE), hWnd,
                                       DlgUserPrefOutputDeviceProc, (LPARAM) &deviceName)) {
                        auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                        SendMessageW(hOutputDeviceList, LB_ADDSTRING, 0, (LPARAM) deviceName.c_str());
                    }
                    break;
                }

                case IDB_DEVICELIST_REMOVE: {
                    auto hOutputDeviceList = GetDlgItem(hWnd, IDC_OUTPUT_DEVICE_LIST);
                    auto curSel = ListBox_GetCurSel(hOutputDeviceList);
                    if (curSel != LB_ERR) {
                        SendMessageW(hOutputDeviceList, LB_DELETESTRING, curSel, 0);
                    }
                    break;
                }

                case IDB_DEVICELIST_UP: {
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

                case IDB_DEVICELIST_DOWN: {
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
                                if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_USERPREF_OUTPUT_DEVICE), hWnd,
                                                   DlgUserPrefOutputDeviceProc, (LPARAM) &deviceName)) {
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
                    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_OUTPUT_LATENCY_OVERRIDE_EDIT), hWnd,
                                   DlgUserPrefDurationOverrideEditProc, (LPARAM) prefPtr);
                }
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}


void driverSettingsGUIThread() {
    CoInitialize(nullptr);

    auto hInstance = GetModuleHandle(nullptr);
    auto hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_USERPREF_EDIT), nullptr, DlgUserPrefEditWndProc);

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

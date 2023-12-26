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
// along with ASIO2WASAPI2.  If not, see <http://www.gnu.org/licenses/>.
//

//
// Created by whyask37 on 2023-12-24.
//

#include "DlgOutputDevice.h"
#include "../res/resource.h"
#include "../utils/WASAPIUtils.h"
#include "../utils/w32StringGetter.h"

#include <vector>

static LPCWSTR listSeparator = L"----------------";

static INT_PTR CALLBACK DlgUserPrefOutputDeviceProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INITDIALOG: {
            auto deviceNameBuffer = reinterpret_cast<std::wstring *>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) deviceNameBuffer);

            auto devices = getIMMDeviceList();
            auto hDeviceList = GetDlgItem(hWnd, IDC_DEVICE);

            std::vector<std::wstring> candidates;

            candidates.emplace_back(L"(default device)");
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

            return FALSE;
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


BOOL PromptDeviceName(HINSTANCE hInstance, HWND hWndParent, std::wstring *deviceName) {
    return DialogBoxParam(
            hInstance,
            MAKEINTRESOURCE(IDD_USERPREF_OUTPUT_DEVICE),
            hWndParent,
            DlgUserPrefOutputDeviceProc,
            (LPARAM) deviceName);
}
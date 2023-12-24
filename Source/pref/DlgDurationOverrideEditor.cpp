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

#include "DlgDurationOverrideEditor.h"
#include "../res/resource.h"
#include <sstream>
#include <string>
#include <regex>
#include "../utils/dlgGetText.h"

static std::wstring trim(std::wstring str) {
    str.erase(str.find_last_not_of(' ') + 1);         //suffixing spaces
    str.erase(0, str.find_first_not_of(' '));       //prefixing spaces
    return str;
}

static INT_PTR CALLBACK DlgUserPrefDurationOverrideEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

BOOL DialogDurationOverrideEditor(HINSTANCE hInstance, HWND hWndParent, UserPrefPtr pref) {
    return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_OUTPUT_LATENCY_OVERRIDE_EDIT), hWndParent,
                          DlgUserPrefDurationOverrideEditProc, (LPARAM) &pref);
}

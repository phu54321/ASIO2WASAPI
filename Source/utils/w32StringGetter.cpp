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

#include "w32StringGetter.h"
#include <vector>

std::wstring getWndText(HWND hWnd) {
    std::vector<wchar_t> v;
    auto len = GetWindowTextLengthW(hWnd) + 1;
    v.resize(len);
    GetWindowTextW(hWnd, v.data(), len);
    return v.data();
}

std::wstring getDlgText(HWND hDlgItem, int id, UINT getLengthMsg, UINT getTextMsg) {
    std::vector<wchar_t> v;
    v.resize(SendMessageW(hDlgItem, getLengthMsg, id, 0) + 1);
    SendMessageW(hDlgItem, getTextMsg, id, (LPARAM) v.data());
    return v.data();
}

std::wstring getResourceString(HINSTANCE hInstance, UINT stringID) {
    const wchar_t *str;
    int stringLength = LoadStringW(hInstance, stringID, (LPWSTR) &str, 0);
    return {str, str + stringLength};
}

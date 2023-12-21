// Copyright (C) 2023 Hyun Woo Park
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


#include "utf8convert.h"
#include <Windows.h>
#include <vector>

// convert UTF-8 string to std::wstring
std::wstring utf8_to_wstring(const std::string &str) {
    int nLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), nullptr, 0);
    std::vector<wchar_t> wb(nLen);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), wb.data(), nLen);
    return {wb.begin(), wb.end()};
}

// convert std::wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring &str) {
    int nLen = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), nullptr, 0, nullptr, nullptr);
    std::vector<char> mb(nLen);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.size(), mb.data(), nLen, nullptr, nullptr);
    return {mb.begin(), mb.end()};
}

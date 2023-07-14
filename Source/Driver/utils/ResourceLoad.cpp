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

#include "ResourceLoad.h"
#include "../res/resource.h"
#include "logger.h"

// Code from https://learn.microsoft.com/en-us/windows/win32/menurc/using-resources

std::vector<uint8_t> loadUserdataResource(
        HMODULE hDLL,
        LPCTSTR resourceIdentifier
) {

    std::vector<uint8_t> ret;

    auto hRes = FindResource(hDLL, resourceIdentifier, MAKEINTRESOURCE(RT_USERDATA));
    if (hRes == nullptr) {
        mainlog->error(TEXT("Failed to find resource {}"), resourceIdentifier);
        return ret;
    }

    auto hResLoad = LoadResource(hDLL, hRes);
    if (hResLoad == nullptr) {
        mainlog->error(TEXT("Failed to load resource {}"), resourceIdentifier);
        return ret;
    }

    auto lpResLock = LockResource(hResLoad);
    if (lpResLock == nullptr) {
        mainlog->error(TEXT("Failed to lock resource {}"), resourceIdentifier);
        return ret;
    }
    auto resourceSize = SizeofResource(hDLL, hRes);
    ret.resize(resourceSize);
    memcpy(ret.data(), lpResLock, resourceSize);

    return ret;
}
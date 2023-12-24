// Copyright (C) 2023 Hyun Woo Park
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



#ifndef TRGKASIO_USERPREF_H
#define TRGKASIO_USERPREF_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <Windows.h>
#include <spdlog/spdlog.h>

struct UserPref {
    int channelCount = 2;
    double clapGain = 0;
    bool throttle = true;
    spdlog::level::level_enum logLevel = spdlog::level::info;
    std::vector<std::wstring> deviceIdList;
    std::map<std::wstring, int> durationOverride;
};

using UserPrefPtr = std::shared_ptr<UserPref>;

UserPrefPtr loadUserPref(LPCTSTR loadRelPath = TEXT("trgkASIO.json"));

void saveUserPref(UserPrefPtr pref, LPCTSTR saveRelPath = TEXT("trgkASIO.json"));

#endif //TRGKASIO_USERPREF_H

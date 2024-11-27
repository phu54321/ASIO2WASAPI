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



#include "UserPref.h"
#include "../utils/json.hpp"
#include "../utils/utf8convert.h"
#include "../utils/homeDirFilePath.h"
#include "../utils/logger.h"
#include <spdlog/spdlog.h>

using json = nlohmann::json;

const wchar_t *defaultDevices[] = {
        L"(default device)"
};

UserPrefPtr loadUserPref(LPCTSTR loadRelPath) {
    FILE *fp = homeDirFOpen(loadRelPath, TEXT("r"));
    auto ret = std::make_shared<UserPref>();
    if (!fp) {
        // use default
        mainlog->info("trgkASIO.json not found. Using default settings");
        for (const wchar_t *device: defaultDevices) {
            ret->deviceIdList.emplace_back(device);
        }

        return ret;
    }

    try {
        auto j = json::parse(fp);

        ret->channelCount = j.value("channelCount", 2);

        // Note:: Declare default log level on logger.cpp
        auto logLevel = j.value("logLevel", "");
        if (logLevel == "trace") ret->logLevel = spdlog::level::trace;
        if (logLevel == "debug") ret->logLevel = spdlog::level::debug;
        if (logLevel == "info") ret->logLevel = spdlog::level::info;
        if (logLevel == "warn") ret->logLevel = spdlog::level::warn;
        if (logLevel == "error") ret->logLevel = spdlog::level::err;

        // additional audio inputs
        ret->clapGain = j.value("clapGain", 0.);
        ret->loopbackInputDevice = utf8_to_wstring(j.value("loopbackInputDevice", ""));
        ret->autoChangeOutputToLoopback = j.value("autoChangeOutputToLoopback", false);

        // audio outputs
        if (!j.contains("deviceId")) {
            for (const wchar_t *device: defaultDevices) {
                ret->deviceIdList.emplace_back(device);
            }
        } else if (j["deviceId"].is_string()) {
            ret->deviceIdList.push_back(utf8_to_wstring(j.value("deviceId", "")));
        } else if (j["deviceId"].is_array()) {
            for (const auto &item: j["deviceId"]) {
                ret->deviceIdList.push_back(utf8_to_wstring(item));
            }
        }

        if (j.contains("durationOverride")) {
            auto &durationOverride = j["durationOverride"];
            if (!durationOverride.is_object()) {
                throw AppException("durationOverride must be an object");
            }

            for (auto it = durationOverride.begin(); it != durationOverride.end(); ++it) {
                std::wstring deviceId = utf8_to_wstring(it.key());
                int override = it.value();
                ret->durationOverride.insert(std::make_pair(deviceId, override));
            }
        }

        return ret;
    } catch (json::exception &e) {
        mainlog->error("JSON parse failed: {}", e.what());
        throw AppException("JSON parse failed");
    }
}


void saveUserPref(const UserPrefPtr &pref, LPCTSTR saveRelPath) {
    FILE *fp = homeDirFOpen(saveRelPath, TEXT("w"));
    if (!fp) {
        // use default
        mainlog->error(TEXT("[saveUserPref] homeDirFOpen failed: {}"), saveRelPath);
        return;
    }

    json j;

    j["channelCount"] = pref->channelCount;
    switch (pref->logLevel) {
        case spdlog::level::trace:
            j["logLevel"] = "trace";
            break;
        case spdlog::level::debug:
            j["logLevel"] = "debug";
            break;
        case spdlog::level::info:
            j["logLevel"] = "info";
            break;
        case spdlog::level::warn:
            j["logLevel"] = "warn";
            break;
        case spdlog::level::err:
            j["logLevel"] = "error";
            break;
        default:;
    }

    {
        j["clapGain"] = pref->clapGain;
        j["loopbackInputDevice"] = wstring_to_utf8(pref->loopbackInputDevice);
        j["autoChangeOutputToLoopback"] = pref->autoChangeOutputToLoopback;
    }

    {
        json jDeviceIdList = json::array();
        for (const auto &s: pref->deviceIdList) {
            jDeviceIdList.push_back(wstring_to_utf8(s));
        }
        j["deviceId"] = jDeviceIdList;

        json jDurationOverride = json::object();
        for (const auto &p: pref->durationOverride) {
            jDurationOverride[wstring_to_utf8(p.first)] = p.second;
        }
        j["durationOverride"] = jDurationOverride;
    }

    fputs(j.dump(2).c_str(), fp);
    fclose(fp);
}

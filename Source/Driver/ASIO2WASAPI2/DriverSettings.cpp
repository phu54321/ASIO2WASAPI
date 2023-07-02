//
// Created by whyask37 on 2023-06-30.
//

#include "ASIO2WASAPI2Impl.h"
#include "../utils/json.hpp"
#include "../utils/utf8convert.h"
#include "../utils/homeDirFilePath.h"
#include "../utils/logger.h"
#include <spdlog/spdlog.h>

using json = nlohmann::json;

DriverSettings loadDriverSettings() {
    FILE *fp = homeDirFOpen(TEXT("ASIO2WASAPI2.json"), TEXT("rb"));
    if (!fp) {
        // use default
        DriverSettings ret;
        mainlog->info("ASIO2WASAPI2.json not found. Using default settings");
        ret.deviceIdList.emplace_back(L"(default)");
        ret.deviceIdList.emplace_back(L"CABLE Input(VB-Audio Virtual Cable)");
        return ret;
    }

    try {
        auto j = json::parse(fp);

        DriverSettings ret;
        ret.nChannels = j.value("channelCount", 2);
        ret.nSampleRate = j.value("sampleRate", 48000);
        ret.bufferSize = j.value("bufferSize", 1024);

        if (j["deviceId"].is_string()) {
            ret.deviceIdList.push_back(utf8_to_wstring(j.value("deviceId", "")));
        } else if (j["deviceId"].is_array()) {
            for (const auto &item: j["deviceId"]) {
                ret.deviceIdList.push_back(utf8_to_wstring(item));
            }
        }
        return ret;
    } catch (json::exception &e) {
        mainlog->error("JSON parse failed: {}", e.what());
        throw AppException("JSON parse failed");
    }
}

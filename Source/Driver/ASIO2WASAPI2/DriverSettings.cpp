//
// Created by whyask37 on 2023-06-30.
//

#include "ASIO2WASAPI2Impl.h"
#include "../utils/logger.h"
#include "../utils/json.hpp"
#include "../utils/utf8convert.h"
#include "../utils/homeDirFilePath.h"

using json = nlohmann::json;

DriverSettings loadDriverSettings() {
    FILE *fp = homeDirFOpen(TEXT("ASIO2WASAPI2.json"), TEXT("rb"));
    if (!fp) {
        throw AppException("Cannot open ASIO2WASAPI2.json");
    }

    auto j = json::parse(fp);

    DriverSettings ret;
    ret.nChannels = j.value("channelCount", 2);
    ret.nSampleRate = j.value("sampleRate", 48000);
    ret.bufferSize = j.value("bufferSize", 1024);
    ret.deviceId = utf8_to_wstring(j.value("deviceId", ""));
    return ret;
}


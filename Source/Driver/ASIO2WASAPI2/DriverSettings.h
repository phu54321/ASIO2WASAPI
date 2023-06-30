//
// Created by whyask37 on 2023-06-30.
//

#ifndef ASIO2WASAPI2_DRIVERSETTINGS_H
#define ASIO2WASAPI2_DRIVERSETTINGS_H

#include <string>
#include <vector>

struct DriverSettings {
    int nChannels = 2;
    int nSampleRate = 48000;
    int bufferSize = 1024;
    std::vector<std::wstring> deviceIdList;
};

DriverSettings loadDriverSettings();

#endif //ASIO2WASAPI2_DRIVERSETTINGS_H

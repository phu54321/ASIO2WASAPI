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


#include "logger.h"
#include "homeDirFilePath.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

std::unique_ptr<spdlog::logger> mainlog;

#ifndef tfopen
#ifndef UNICODE
#define tfopen fopen
#else
#define tfopen _wfopen
#endif
#endif

void initMainLog() {
    std::vector<spdlog::sink_ptr> sinks;

    // See https://github.com/gabime/spdlog/issues/2408
    // OutputDebugString is SLOW, so this will only work with debugger attached.
    {
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        msvc_sink->set_pattern("[%Y%m%d %H:%M:%S.%f] [%l%$] %v");
        sinks.push_back(msvc_sink);
    }

    {
        auto logFileName = homeDirFilePath(TEXT("ASIO2WASAPI2.log"));
        FILE *rf = tfopen(logFileName.c_str(), TEXT("rb"));
        if (rf) {
            fclose(rf);
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, false);
            file_sink->set_pattern("[%Y%m%d %H:%M:%S.%f] [%l%$] %v");
            sinks.push_back(file_sink);
        }
    }

    mainlog = std::make_unique<spdlog::logger>("main_logger", sinks.begin(), sinks.end());
    mainlog->set_level(spdlog::level::info);
}

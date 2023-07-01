//
// Created by whyask37 on 2023-07-01.
//

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
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName, true);
            file_sink->set_pattern("[%Y%m%d %H:%M:%S.%f] [%l%$] %v");
            sinks.push_back(file_sink);
        }
    }

    mainlog = std::make_unique<spdlog::logger>("main_logger", sinks.begin(), sinks.end());
    mainlog->set_level(spdlog::level::trace);
}
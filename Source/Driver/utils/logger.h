#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include "accurateTime.h"
#include "AppException.h"

extern std::unique_ptr<spdlog::logger> mainlog;

void initMainLog();

#define runtime_check(expr, msg, ...) \
    if (!(expr)) {                  \
        auto s = fmt::format(msg, __VA_ARGS__); \
        throw AppException(s); \
    }

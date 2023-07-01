#pragma once

#include <spdlog/spdlog.h>
#include <memory>

extern std::unique_ptr<spdlog::logger> mainlog;
void initMainLog();

class FuncTraceHelper {
public:
    FuncTraceHelper(const char *funcname) : _funcname(funcname) {
        mainlog->trace("Entering {}", funcname);
    }

    ~FuncTraceHelper() {
        mainlog->trace("Entering {}", _funcname);
    }

private:
    const char *_funcname;
};

#define SPDLOG_TRACE_FUNC \
    FuncTraceHelper _fth(__FUNCTION__)

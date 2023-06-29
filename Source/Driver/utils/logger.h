#pragma once

#include <stdio.h>
#include <stdarg.h>

enum LogLevel {
    trace = 0,
    debug,
    info,
    warn,
    error
};

class Logger {
private:
    Logger();

    ~Logger();

    static Logger &getInstance();

    Logger(const Logger &) = delete;

    Logger &operator=(const Logger &) = delete;

public:
    static void setMinimumOutputLevel(LogLevel minimumOutputLevel);

    // wchar_t*
    static void trace(const wchar_t *fmt, ...);

    static void debug(const wchar_t *fmt, ...);

    static void info(const wchar_t *fmt, ...);

    static void warn(const wchar_t *fmt, ...);

    static void error(const wchar_t *fmt, ...);

    // char*
    static void trace(const char *fmt, ...);

    static void debug(const char *fmt, ...);

    static void info(const char *fmt, ...);

    static void warn(const char *fmt, ...);

    static void error(const char *fmt, ...);

private:
    void logV(LogLevel level, const wchar_t *fmt, va_list args);
    void logV(LogLevel level, const char *fmt, va_list args);

private:
    FILE *outputFile;
    LogLevel _minimumOutputLevel = LogLevel::info;
};

class FuncTraceHelper {
public:
    FuncTraceHelper(const char *funcname);

    ~FuncTraceHelper();

private:
    const char *funcname;
};

#define LOGGER_TRACE_FUNC \
    FuncTraceHelper _fth(__FUNCTION__)

#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <ShlObj.h>
#include <iomanip>
#include <chrono>
#include "logger.h"
#include "utf8convert.h"
///

#ifndef UNICODE
typedef std::string String;
#define tfopen fopen
#else
typedef std::wstring String;
#define tfopen _wfopen
#endif

String getHomeDir() {
    TCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return path;
    }
    return TEXT("");
}

void AddTrailingSeparator(String &str) {
    if (str.length() == 0 || str[str.length() - 1] != '\\')
        str += TEXT("\\");
}


///


Logger::Logger() : outputFile(nullptr) {
    auto homeDir = getHomeDir();
    AddTrailingSeparator(homeDir);
    auto logFilePath = homeDir + TEXT("ASIO2WASAPI2.log");

    // Only open log file if it exists
    FILE *rf = tfopen(logFilePath.c_str(), TEXT("rb"));
    if (rf) {
        fclose(rf);
        outputFile = tfopen(logFilePath.c_str(), TEXT("wb"));
    }
}

Logger::~Logger() {
    if (outputFile) {
        fclose(outputFile);
        outputFile = NULL;
    }
}

Logger &Logger::getInstance() {
    static Logger inst;
    return inst;
}

void Logger::setMinimumOutputLevel(LogLevel minimumOutputLevel) {
    auto &inst = getInstance();
    inst._minimumOutputLevel = minimumOutputLevel;
}

//

#define LOGGER_CREATE_LOGLEVEL_IMPL(level) \
    void Logger::level(const wchar_t* format, ...) { \
        Logger& logger = getInstance(); \
        va_list args; \
        va_start(args, format); \
        logger.logV(LogLevel::level, format, args); \
        va_end(args); \
    } \
    \
    void Logger::level(const char* format, ...) {    \
        Logger& logger = getInstance(); \
        va_list args; \
        va_start(args, format); \
        logger.logV(LogLevel::level, format, args); \
        va_end(args); \
    } \


LOGGER_CREATE_LOGLEVEL_IMPL(trace)

LOGGER_CREATE_LOGLEVEL_IMPL(debug)

LOGGER_CREATE_LOGLEVEL_IMPL(info)

LOGGER_CREATE_LOGLEVEL_IMPL(warn)

LOGGER_CREATE_LOGLEVEL_IMPL(error)

//

static void getCurrentTimestamp(char *out) {
    auto currentTime = std::chrono::system_clock::now();
    auto transformed = currentTime.time_since_epoch().count() / 1000000;
    auto millis = transformed % 1000;

    auto tt = std::chrono::system_clock::to_time_t(currentTime);
    auto timeinfo = localtime(&tt);

    auto outP = strftime(out, 80, "%F %H:%M:%S", timeinfo);
    sprintf(out + outP, ":%03d", (int) millis);
}

static void putTimestamp(FILE *outputFile) {
    char timestamp[80];
    getCurrentTimestamp(timestamp);
    fputc('[', outputFile);
    fputs(timestamp, outputFile);
    fputc(']', outputFile);
}

static void putLogLevel(FILE *outputFile, LogLevel level) {
    const char *levelString = nullptr;
    switch (level) {
        case LogLevel::trace:
            levelString = "[TRACE]";
            break;
        case LogLevel::debug:
            levelString = "[DEBUG]";
            break;
        case LogLevel::info:
            levelString = "[INFO]";
            break;
        case LogLevel::warn:
            levelString = "[WARN]";
            break;
        case LogLevel::error:
            levelString = "[ERROR]";
            break;
        default:
            levelString = "[unk]";
    }
    fputs(levelString, outputFile);
    fputc(' ', outputFile);
}

void Logger::logV(LogLevel level, const wchar_t *format, va_list args) {
    if (level < this->_minimumOutputLevel) return;

    wchar_t wBuffer[2048];
    _vsnwprintf(wBuffer, 2048, format, args);
    wBuffer[2047] = '\0';
    OutputDebugStringW(wBuffer);

    if (outputFile) {
        putTimestamp(outputFile);
        putLogLevel(outputFile, level);
        auto utf8String = wstring_to_utf8(wBuffer);
        fputs(utf8String.c_str(), outputFile);
        fputc('\n', outputFile);
    }
}


void Logger::logV(LogLevel level, const char *format, va_list args) {
    if (level < this->_minimumOutputLevel) return;

    char buffer[2048];  // UTF-8 encoding
    vsnprintf(buffer, 2048, format, args);
    buffer[2047] = '\0';
    OutputDebugStringA(buffer);

    if (outputFile) {
        putTimestamp(outputFile);
        putLogLevel(outputFile, level);
        fputs(buffer, outputFile);
        fputc('\n', outputFile);
    }
}


// %S: const char* on unicode space
FuncTraceHelper::FuncTraceHelper(const char *funcname) : funcname(funcname) {
    Logger::trace(L"entering %S", funcname);
}

FuncTraceHelper::~FuncTraceHelper() {
    Logger::trace(L"leaving %S", funcname);
}

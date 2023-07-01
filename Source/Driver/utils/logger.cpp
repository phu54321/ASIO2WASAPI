#include "logger.h"

#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <chrono>
#include "utf8convert.h"
#include "homeDirFilePath.h"

#ifndef tfopen
#ifndef UNICODE
#define tfopen fopen
#else
#define tfopen _wfopen
#endif
#endif


Logger::Logger() : outputFile(nullptr) {
    auto logFileName = homeDirFilePath(TEXT("ASIO2WASAPI2.log"));

    // Only open log file if it exists
    FILE *rf = tfopen(logFileName.c_str(), TEXT("rb"));
    if (rf) {
        fclose(rf);
        outputFile = tfopen(logFileName.c_str(), TEXT("wb"));
        setbuf(outputFile, NULL);
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
    auto transformed = currentTime.time_since_epoch().count();
    auto micros = transformed % 1000000;

    auto tt = std::chrono::system_clock::to_time_t(currentTime);
    auto timeinfo = localtime(&tt);

    auto outP = strftime(out, 80, "%F %H:%M:%S", timeinfo);
    sprintf(out + outP, ":%06d", (int) micros);
}

static void putTimestamp(FILE *outputFile) {
    char timestamp[80];
    getCurrentTimestamp(timestamp);
    fputc('[', outputFile);
    fputs(timestamp, outputFile);
    fputs("] ", outputFile);
}

static void putThreadId(FILE *outputFile) {
    // See https://stackoverflow.com/questions/7432100/how-to-get-integer-thread-id-in-c11
    size_t threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    fprintf(outputFile, "[Thread 0x%016zX] ", threadId);
}

static void putLogLevel(FILE *outputFile, LogLevel level) {
    const char *levelString;
    switch (level) {
        case LogLevel::trace:
            levelString = "[trace]";
            break;
        case LogLevel::debug:
            levelString = "[debug]";
            break;
        case LogLevel::info:
            levelString = "[info] ";
            break;
        case LogLevel::warn:
            levelString = "[warn] ";
            break;
        case LogLevel::error:
            levelString = "[error]";
            break;
        default:
            levelString = "???????";
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
        std::lock_guard<std::mutex> lock(_fileMutex);
        putTimestamp(outputFile);
        putLogLevel(outputFile, level);
        putThreadId(outputFile);
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
        std::lock_guard<std::mutex> lock(_fileMutex);
        putTimestamp(outputFile);
        putLogLevel(outputFile, level);
        putThreadId(outputFile);
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

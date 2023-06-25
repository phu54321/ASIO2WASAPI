#include "stdafx.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <Shlobj.h>
#include "logger.h"

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
}

void AddTrailingSeparator(String& str)
{
	if (str.length() == 0 || str[str.length() - 1] != '\\')
		str += TEXT("\\");
}


///


Logger::Logger(): outputFile(nullptr){
	auto homeDir = getHomeDir();
	AddTrailingSeparator(homeDir);
	auto logFilePath = homeDir + TEXT("ASIO2WASAPI2.log");

	// Only open log file if it exists
	FILE* rf = tfopen(logFilePath.c_str(), TEXT("rb"));
	if (rf) {
		fclose(rf);
		outputFile = tfopen(logFilePath.c_str(), TEXT("wb"));
		setbuf(outputFile, NULL);
	}
}

Logger::~Logger() {
	if (outputFile) {
		fclose(outputFile);
		outputFile = NULL;
	}
}

Logger& Logger::getInstance() {
	static Logger inst;
	return inst;
}

void Logger::setOutputLevel(LogLevel level) {
	auto& inst = getInstance();
	inst.level = level;
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

LOGGER_CREATE_LOGLEVEL_IMPL(trace)
LOGGER_CREATE_LOGLEVEL_IMPL(debug)
LOGGER_CREATE_LOGLEVEL_IMPL(info)
LOGGER_CREATE_LOGLEVEL_IMPL(warn)
LOGGER_CREATE_LOGLEVEL_IMPL(error)

void Logger::log(LogLevel level, const wchar_t* format, ...) {
	Logger& logger = getInstance();
	va_list args;
	va_start(args, format);
	logger.logV(level, format, args);
	va_end(args);
}

//

void Logger::logV(LogLevel level, const wchar_t* format, va_list args) {
	if (level < this->level) return;

	wchar_t wBuffer[2048];
	char buffer[2048];  // UTF-8 encoding
	_vsnwprintf(wBuffer, 2048, format, args);
	wBuffer[2047] = '\0';

	int nLen = WideCharToMultiByte(CP_UTF8, 0, wBuffer, lstrlenW(wBuffer), NULL, 0, NULL, NULL);
	WideCharToMultiByte(CP_UTF8, 0, wBuffer, lstrlenW(wBuffer), buffer, nLen, NULL, NULL);

	OutputDebugStringW(wBuffer);

	const char* levelString = nullptr;
	if (outputFile) {
		switch (level) {
		case LogLevel::trace: fputs("[TRACE] ", outputFile); break;
		case LogLevel::debug: fputs("[DEBUG] ", outputFile); break;
		case LogLevel::info: fputs("[INFO] ", outputFile); break;
		case LogLevel::warn: fputs("[WARN] ", outputFile); break;
		case LogLevel::error: fputs("[ERROR] ", outputFile); break;
		}

		fputs(buffer, outputFile);
		fputc('\n', outputFile);
	}
}

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
	static Logger& getInstance();
	Logger(const Logger&) = delete;
	Logger& operator= (const Logger&) = delete;

public:
	static void setOutputLevel(LogLevel minLevel);
	static void trace(const wchar_t* fmt, ...);
	static void debug(const wchar_t* fmt, ...);
	static void info(const wchar_t* fmt, ...);
	static void warn(const wchar_t* fmt, ...);
	static void error(const wchar_t* fmt, ...);
	static void log(LogLevel level, const wchar_t* fmt, ...);

private:
	void logV(LogLevel level, const wchar_t* fmt, va_list args);

private:
	FILE* outputFile;
	LogLevel level = LogLevel::info;
};

// From http://blog.redjini.com/188
// Tool to convert __function__ to unicode
#define WIDEN(x)           L ## x
#define WIDEN2(x)         WIDEN(x)
#define __WFUNCTION__ WIDEN2(__FUNCTION__)

class FuncTraceHelper {
public:
	FuncTraceHelper(const wchar_t* funcname);
	~FuncTraceHelper();

private:
	const wchar_t* funcname;
};

#define LOGGER_TRACE_FUNC \
	FuncTraceHelper _fth(__WFUNCTION__)

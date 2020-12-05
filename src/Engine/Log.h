#pragma once

enum class LogSeverity
{
	INFO,
	WARNING,
	ERR
};

void Log(LogSeverity a_logSeverity, const char* a_fileName, int a_lineNumber, const char* message, ...);

#if defined(_DEBUG)
#define LOG(_LogSeverity, ...) Log(_LogSeverity, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_IF(condition, _LogSeverity, ...) if(!(condition)) Log(_LogSeverity, __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG(_LogSeverity, ...) void();
#define LOG_IF(condition, _LogSeverity, ...) if(condition) {}
#endif
#include "Log.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <string>
#define BUFFER_SIZE 1024
#include <thread>
#include <mutex>

static std::mutex mtx;

const char* GetFileNameFromPath(const char* path)
{
	std::string str = path;
	size_t pos = str.find_last_of("\\");
	return path + pos+1;
}

void Log(LogSeverity a_eLogSeverity, const char* a_fileName, int a_lineNumber, const char* message, ...)
{
	static char buffer[BUFFER_SIZE] = {};

	static const char* LogSevereStr[] = {
		"INFO | ",
		"WARN | ",
		"ERR | "
	};

	mtx.lock();

	uint32_t offset = 0, index = 0;
#if defined(_WIN32)
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// reset console text color
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	switch (a_eLogSeverity)
	{
	case LogSeverity::INFO:
		index = 0;
		break;
	case LogSeverity::WARNING:
		index = 1;
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);
		break;
	case LogSeverity::ERR:
		index = 2;
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
		break;
	};
#endif

	offset = (uint32_t)strlen(LogSevereStr[index]);
	memcpy(buffer, LogSevereStr[index], offset);

	// add file name and line number
	const char* filename = GetFileNameFromPath(a_fileName);
	std::string str = filename;
	str += "(" + std::to_string(a_lineNumber) + ") | ";
	memcpy(buffer+offset, str.c_str(), str.size());
	offset += (uint32_t)str.size();

	va_list args;
	va_start(args, message);
	vsnprintf(buffer+offset, BUFFER_SIZE, message, args);
	va_end(args);

	printf("%s\n", buffer);

	memset(buffer, 0, sizeof(char) * BUFFER_SIZE);

#if defined(_WIN32)
	if (a_eLogSeverity == LogSeverity::ERR)
	{
		assert(0);
	}
#endif

	mtx.unlock();
}
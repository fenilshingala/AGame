#include "../FileSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>     // For access().
#include <sys/types.h>  // For stat().
#include <sys/stat.h>   // For stat().
#include <stdio.h>
#include "../../Log.h"

void InitFileSystem(void* a_PlatformData)
{}

bool ExistDirectory(const char* a_sDirectoryPath)
{
	LOG_IF(a_sDirectoryPath, LogSeverity::ERR, "Directory name empty!");

	struct stat status;
	stat(a_sDirectoryPath, &status);

	return (status.st_mode & S_IFDIR) != 0;
}

void CreateDirecroty(const char* a_sDirectoryName)
{
	LOG_IF(a_sDirectoryName, LogSeverity::ERR, "Directory name empty!");
	
	if (CreateDirectoryA(a_sDirectoryName, NULL) == 0)
	{
		if (ERROR_ALREADY_EXISTS == GetLastError())
		{
			LOG(LogSeverity::ERR, "Directory already exist!");
		}
		else if (ERROR_PATH_NOT_FOUND == GetLastError())
		{
			LOG(LogSeverity::ERR, "Path not found!");
		}
	}
}

FileHandle OpenFile(const char* a_sFilename, const char* a_sMode)
{
	LOG_IF(a_sFilename, LogSeverity::ERR, "Empty File Name");
	LOG_IF(a_sMode, LogSeverity::ERR, "Empty File Mode");

	FILE* pFile = fopen(a_sFilename, a_sMode);
	LOG_IF(pFile, LogSeverity::ERR, "Could not open file %s", a_sFilename);

	return (FileHandle)pFile;
}

void CloseFile(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	fclose((FILE*)a_Handle);
}

int FileRead(FileHandle a_Handle, char** a_ppBuffer, uint32_t a_uLength)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	LOG_IF(*a_ppBuffer, LogSeverity::ERR, "Value at buffer is NULL");
	return (int)fread(*a_ppBuffer, 1, a_uLength, (FILE*)a_Handle);
}

uint32_t FileSize(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");

	FILE* pFile = (FILE*)a_Handle;
	fseek(pFile, 0L, SEEK_END);
	uint32_t size = ftell(pFile);
	fseek(pFile, 0L, SEEK_SET);
	return size;
}

void FileWriteLine(FileHandle a_Handle, const char* a_sBuffer)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	fputs(a_sBuffer, (FILE*)a_Handle);
}

long FileTell(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	return ftell((FILE*)a_Handle);
}

void FileSeek(FileHandle a_Handle, long a_lOffset, int a_iOrigin)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	fseek((FILE*)a_Handle, a_lOffset, a_iOrigin);
}

int IsEndOfFile(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	return feof((FILE*)a_Handle);
}
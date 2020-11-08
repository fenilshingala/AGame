#include "../FileSystem.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h> // fopen, fclose
#include <assert.h>

void InitFileSystem(void* _platformData)
{}

void CreateDirecroty(const char* _directoryName)
{
	assert(_directoryName);
	
	if (CreateDirectoryA(_directoryName, NULL) == 0)
	{
		if (ERROR_ALREADY_EXISTS == GetLastError())
		{
			// LOG
		}
		else if (ERROR_PATH_NOT_FOUND == GetLastError())
		{
			// LOG
		}
	}
}

FileHandle OpenFile(const char* _filename, const char* _mode)
{
	FILE* pFile = fopen(_filename, _mode);
	assert(pFile);
	return (FileHandle)pFile;
}

void CloseFile(FileHandle _handle)
{
	assert(_handle);
	fclose((FILE*)_handle);
}

void FileReadLine(FileHandle _handle, char** _ppBuffer)
{
	assert(_handle);
	fgets(*_ppBuffer, 256, (FILE*)_handle);
}

void FileWriteLine(FileHandle _handle, const char* buffer)
{
	assert(_handle);
	fputs(buffer, (FILE*)_handle);
}

long FileTell(FileHandle _handle)
{
	assert(_handle);
	return ftell((FILE*)_handle);
}

void FileSeek(FileHandle _handle, long _offset, int _origin)
{
	assert(_handle);
	fseek((FILE*)_handle, _offset, _origin);
}
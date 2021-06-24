#pragma once

#include <stdint.h>
typedef void* FileHandle;

void InitFileSystem(void* a_PlatformData);
void CreateDirecroty(const char* a_sDirectoryName);
bool ExistDirectory(const char* a_sDirectoryPath);
FileHandle OpenFile(const char* a_sFilename, const char* a_sMode);
void CloseFile(FileHandle a_Handle);
int FileRead(FileHandle a_Handle, char** a_ppBuffer, uint32_t a_uLength);
uint32_t FileSize(FileHandle a_Handle);
void FileWriteLine(FileHandle a_Handle, const char* a_sBuffer);
long FileTell(FileHandle a_Handle);
void FileSeek(FileHandle a_Handle, long a_lOffset, int a_iOrigin);
int IsEndOfFile(FileHandle a_Handle);
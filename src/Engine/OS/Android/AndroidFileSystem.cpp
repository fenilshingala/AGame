#include "../FileSystem.h"

#include "android_native_app_glue.h"
#include <sys/stat.h>
#include <android/asset_manager.h>
#include <stdio.h> // fopen, fclose
#include <assert.h>

typedef void* FileHandle;
static AAssetManager* assetManager = nullptr;

void InitFileSystem(void* _platformData)
{
	struct android_app* app = (struct android_app*)_platformData;
	assetManager = app->activity->assetManager;
}

void CreateDirecroty(const char* _directoryName)
{
	assert(_directoryName);
	
}

FileHandle OpenFile(const char* _filename, const char* _mode)
{
	AAsset* asset = AAssetManager_open(assetManager, _filename, AASSET_MODE_BUFFER);
	assert(asset);
	return asset;
}

void CloseFile(FileHandle _handle)
{
	assert(_handle);
	AAsset_close((AAsset*)_handle);
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
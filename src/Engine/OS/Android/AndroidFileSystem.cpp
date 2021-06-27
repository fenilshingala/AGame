#include "../../FileSystem.h"
#include "../../Log.h"

#include "android_native_app_glue.h"
#include <sys/stat.h>
#include <android/asset_manager.h>

typedef void* FileHandle;
static AAssetManager* assetManager = nullptr;

void InitFileSystem(void* a_PlatformData)
{
	LOG_IF(a_PlatformData, LogSeverity::ERR, "android_app* is NULL");
	struct android_app* app = (struct android_app*)a_PlatformData;
	assetManager = app->activity->assetManager;
}

FileHandle FileOpen(const char* a_sFilename, const char* a_sMode)
{
	LOG_IF(a_sFilename, LogSeverity::ERR, "Empty File Name");
	LOG_IF(a_sMode, LogSeverity::ERR, "Empty File Mode");

	AAsset* asset = AAssetManager_open(assetManager, a_sFilename, AASSET_MODE_BUFFER);
	LOG_IF(asset, LogSeverity::ERR, "Could not open file %s", a_sFilename);
	
	return asset;
}

void FileClose(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	AAsset_close((AAsset*)a_Handle);
}

int FileRead(FileHandle a_Handle, char** a_ppBuffer, uint32_t a_uLength)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	LOG_IF(*a_ppBuffer, LogSeverity::ERR, "Value at buffer is NULL");

	AAsset* asset = (AAsset*)a_Handle;
	return AAsset_read(asset, (*a_ppBuffer), a_uLength);
}

uint32_t FileSize(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");

	AAsset* asset = (AAsset*)a_Handle;
	return AAsset_getLength(asset);
}

// YET TO IMPLEMENT
void CreateDirecroty(const char* a_sDirectoryName)
{
	LOG_IF(a_sDirectoryName, LogSeverity::ERR, "Empty Directory Name");
	return;
}

bool ExistDirectory(const char* a_sDirectoryPath)
{
	LOG_IF(a_sDirectoryPath, LogSeverity::ERR, "Empty Directory Name");
	return false;
}

void FileWriteLine(FileHandle a_Handle, const char* a_sBuffer)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	return;
}
//

long FileTell(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	AAsset* asset = (AAsset*)a_Handle;
	return (AAsset_getLength(asset) - AAsset_getRemainingLength(asset));
}

void FileSeek(FileHandle a_Handle, long a_lOffset, int a_iOrigin)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	AAsset_seek((AAsset*)a_Handle, a_lOffset, a_iOrigin);
}

int IsEndOfFile(FileHandle a_Handle)
{
	LOG_IF(a_Handle, LogSeverity::ERR, "File Handle is NULL");
	return (AAsset_getRemainingLength((AAsset*)a_Handle) > 0 ? 1 : 0);
}
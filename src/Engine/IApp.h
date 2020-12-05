#pragma once

class IApp
{
public:
	virtual void Init() = 0;
	virtual void Exit() = 0;
	virtual void Load() = 0;
	virtual void Unload() = 0;
	virtual void Update() = 0;
};

#if defined(_WIN32)

int WindowsMain(int argc, char** argv, IApp* pApp);

#define APP_MAIN(Application)							\
int main(int argc, char** argv)							\
{														\
	Application instance;								\
	return WindowsMain(argc, argv, &instance);			\
}

#endif

#if defined(__ANDROID_API__)

#include "OS/Android/android_native_app_glue.h"
void AndroidMain(struct android_app* a_pAndroidApp, IApp* a_pApp);

#define APP_MAIN(Application)							\
void android_main(struct android_app* app)				\
{														\
	Application instance;								\
	AndroidMain(app, &instance);						\
}

#endif
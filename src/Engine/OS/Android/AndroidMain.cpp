#include "../../IApp.h"

static IApp* pCurrentApp = nullptr;
static bool initialized = false;

void InitFileSystem(void* _platformData);

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        // The window is being shown, get it ready.
        InitFileSystem(app);
        ::pCurrentApp->Init();
        initialized = true;
        break;
    case APP_CMD_TERM_WINDOW:
        // The window is being hidden or closed, clean it up.
        ::pCurrentApp->Exit();
        break;
    default:
        break;
    }
}

void AndroidMain(struct android_app* _androidApp, IApp* _pApp)
{
    ::pCurrentApp = _pApp;
	_androidApp->onAppCmd = handle_cmd;
    
    int events;
    android_poll_source* source;
    do {
        if (ALooper_pollAll(initialized ? 1 : 0, nullptr, &events,
            (void**)&source) >= 0) {
            if (source != NULL) source->process(_androidApp, source);
        }

        _pApp->Update();

    } while (_androidApp->destroyRequested == 0);
}
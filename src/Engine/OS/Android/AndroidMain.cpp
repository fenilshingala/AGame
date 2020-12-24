#include "../../IApp.h"
#include "../../IPlatform.h"
#include <unistd.h>

static android_app* pAndroidApp = nullptr;
static bool ready = false;
static bool initialized = false;
static struct Window* pWindow = nullptr;
static struct ANativeWindow* pNativeWindow = nullptr;

void InitFileSystem(void* _platformData);

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
    {
        // The window is being shown, get it ready.
        pNativeWindow = pAndroidApp->window;
        ready = true;
        break;
    }
    case APP_CMD_TERM_WINDOW:
    {
        // The window is being hidden or closed, clean it up.
        pWindow->pApp->Unload();
        ready = false;
        break;
    }
    case APP_CMD_WINDOW_RESIZED:
    {
        int32_t width = ANativeWindow_getWidth(app->window);
        int32_t height = ANativeWindow_getHeight(app->window);
        if (pWindow->width != width || pWindow->height != height)
        {
            pWindow->width = width;
            pWindow->height = height;
            pWindow->pApp->Unload();
            pWindow->pApp->Load();
        }
        break;
    }
    default:
    {
        break;
    }
    }
}

void InitWindow(Window* a_pWindow)
{
    pWindow = a_pWindow;
    pWindow->pWindowHandle = pNativeWindow;
}

void ExitWindow(Window* a_pWindow)
{}

bool WindowShouldClose()
{
    return pAndroidApp->destroyRequested;
}

void AndroidMain(struct android_app* a_pAndroidApp, IApp* a_pApp)
{
    ::pAndroidApp = a_pAndroidApp;
    a_pAndroidApp->onAppCmd = handle_cmd;
    
    InitFileSystem(a_pAndroidApp);

    int events;
    android_poll_source* source;
    do
    {
        if (ALooper_pollAll(ready ? 1 : 0, nullptr, &events,
            (void**)&source) >= 0) {
            if (source != NULL) source->process(a_pAndroidApp, source);
        }
        if (!ready && !initialized)
        {
            usleep(1);
            continue;
        }

        if (ready && !initialized)
        {
            a_pApp->Init();
            a_pApp->Load();
            initialized = true;
        }

        a_pApp->Update();

    } while (a_pAndroidApp->destroyRequested == 0);

    a_pApp->Unload();
    a_pApp->Exit();
}
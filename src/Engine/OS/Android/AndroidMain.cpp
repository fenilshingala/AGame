#include "../../IApp.h"
#include "../../IPlatform.h"
#include <unistd.h>

static android_app* pAndroidApp = nullptr;
static bool ready = false;
static bool initialized = false;
static struct Window* pWindow = nullptr;
static struct ANativeWindow* pNativeWindow = nullptr;

void InitFileSystem(void* _platformData);

// Touch inputs
static bool isTouch = false;
static float touchLocationX = TOUCH_LOCATION_INVALID,
             touchLocationY = TOUCH_LOCATION_INVALID;

bool UpdateWindowDimensions(Window* a_pWindow)
{
    ANativeWindow* window = (ANativeWindow*)a_pWindow->pWindowHandle;
    int32_t width = ANativeWindow_getWidth(window);
    int32_t height = ANativeWindow_getHeight(window);
    if (pWindow->width != width || pWindow->height != height)
    {
        pWindow->width = width;
        pWindow->height = height;
        return true;
    }
}

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
    {
        // The window is being shown, get it ready.
        pNativeWindow = pAndroidApp->window;
        ready = true;
        if (initialized)
            pWindow->pApp->Load();
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
        if(UpdateWindowDimensions(pWindow))
        {
            pWindow->pApp->Unload();
            pWindow->pApp->Load();
        }
        break;
    }
    case APP_CMD_GAINED_FOCUS:
    {
        pWindow->minimized = false;
        break;
    }
    case APP_CMD_LOST_FOCUS:
    {
        pWindow->minimized = true;
        pWindow->reset = true;
        break;
    }
    default:
    {
        break;
    }
    }
}

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
    switch (AInputEvent_getType(event))
    {
    /*case AINPUT_EVENT_TYPE_KEY:
    {
        break;
    }*/
    case AINPUT_EVENT_TYPE_MOTION:
    {
        switch (AMotionEvent_getAction(event))
        {
        case AMOTION_EVENT_ACTION_DOWN:
            isTouch = true;
            touchLocationX = AMotionEvent_getX(event, 0);
            touchLocationY = AMotionEvent_getY(event, 0);
            break;

        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            isTouch = false;
            touchLocationX = TOUCH_LOCATION_INVALID;
            touchLocationY = TOUCH_LOCATION_INVALID;
            break;

        case AMOTION_EVENT_ACTION_MOVE:
            touchLocationX = AMotionEvent_getX(event, 0);
            touchLocationY = AMotionEvent_getY(event, 0);
            break;
        default:
            break;
        }
        
        break;
    }
    /*case AINPUT_EVENT_TYPE_FOCUS:
    {
        break;
    }*/
    default:
        break;
    }

    return 0;
}

void InitWindow(Window* a_pWindow)
{
    pWindow = a_pWindow;
    pWindow->pWindowHandle = pNativeWindow;
    pWindow->width = ANativeWindow_getWidth(pNativeWindow);
    pWindow->height = ANativeWindow_getHeight(pNativeWindow);
}

void ExitWindow(Window* a_pWindow)
{}

bool WindowShouldClose()
{
    int events;
    android_poll_source* source;
    if (ALooper_pollAll(ready ? 1 : 0, nullptr, &events,
        (void**)&source) >= 0) {
        if (source != NULL)
            source->process(::pAndroidApp, source);
    }
    return pAndroidApp->destroyRequested;
}

bool IsTouch()
{
    return isTouch;
}

void GetTouchLocation(float* x, float* y)
{
    *x = touchLocationX;
    *y = touchLocationY;
}

void AndroidMain(struct android_app* a_pAndroidApp, IApp* a_pApp)
{
    ::pAndroidApp = a_pAndroidApp;
    a_pAndroidApp->onAppCmd = handle_cmd;
    a_pAndroidApp->onInputEvent = engine_handle_input;

    InitFileSystem(a_pAndroidApp);

    int events;
    android_poll_source* source;
    do
    {
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
    } while (!initialized && !WindowShouldClose());

    a_pApp->Update();

    if(ready)
        a_pApp->Unload();
    if(initialized)
        a_pApp->Exit();
}
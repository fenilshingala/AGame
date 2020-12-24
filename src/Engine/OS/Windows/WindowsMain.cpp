#include "../../IApp.h"
#include "../../IPlatform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static struct Window* pWindow = nullptr;
static bool windowIsResizing = false;
static bool needsResize = false;

void UpdateWindowDimensions(Window* a_pWindow)
{
	HWND hwnd = (HWND)a_pWindow->pWindowHandle;
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	uint32_t width = uint32_t(clientRect.right - clientRect.left);
	uint32_t height = uint32_t(clientRect.bottom - clientRect.top);

	if (pWindow->width != width || pWindow->height != height)
	{
		pWindow->width = width;
		pWindow->height = height;
		needsResize = true;
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!pWindow || hwnd != pWindow->pWindowHandle)
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_DESTROY:
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_ENTERSIZEMOVE:
	{	
		windowIsResizing = true;
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		windowIsResizing = false;
		if (!pWindow->fullScreen)
		{
			UpdateWindowDimensions(pWindow);
		}
		break;
	}
	case WM_SIZE:
	{
		switch (wParam)
		{
		case SIZE_RESTORED:
		case SIZE_MAXIMIZED:
			pWindow->minimized = false;
			if (!pWindow->fullScreen && !windowIsResizing)
			{
				UpdateWindowDimensions(pWindow);
			}
			break;
		case SIZE_MINIMIZED:
			pWindow->minimized = true;
			break;
		default:
			break;
		}
	}
	default:
	{
		break;
	}
	}

	if (pWindow->pApp && !windowIsResizing && needsResize)
	{
		pWindow->pApp->Unload();
		pWindow->pApp->Load();
		needsResize = false;
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void InitWindow(Window* a_pWindow)
{
	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Window Class";
	HINSTANCE hInstance =  GetModuleHandle(NULL);

	WNDCLASS wc = { };
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		L"My Window",					// Window text
		WS_OVERLAPPEDWINDOW,            // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL)
	{
		// LOG Error Could not create a window
		return;
	}

	a_pWindow->pWindowHandle = hwnd;
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	uint32_t width = uint32_t(clientRect.right - clientRect.left);
	uint32_t height = uint32_t(clientRect.bottom - clientRect.top);
	a_pWindow->width = width;
	a_pWindow->height = height;
	::pWindow = a_pWindow;

	ShowWindow(hwnd, SW_SHOW);
}

void ExitWindow(Window* a_pWindow)
{
	DestroyWindow((HWND)a_pWindow->pWindowHandle);
	WindowShouldClose();
}

bool WindowShouldClose()
{
	MSG msg;
	msg.message = NULL;
	bool quit = false;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (WM_CLOSE == msg.message || WM_QUIT == msg.message)
			quit = true;
	}

	return quit;
}

int WindowsMain(int argc, char** argv, IApp* pApp)
{
	pApp->Init();
	pApp->Load();
	pApp->Update();
	pApp->Unload();
	pApp->Exit();

	return 0;
}
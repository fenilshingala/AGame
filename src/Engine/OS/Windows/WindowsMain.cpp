#include "../../App.h"
#include "../../Platform.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// WINDOW
static struct Window* pWindow = nullptr;
static bool windowIsResizing = false;
static bool needsResize = false;

// KEYBOARD
static uint16_t mKeyStates[MAX_KEYS] = { 0 };
static uint32_t currentCharCount = 0;
static char mCharBuffer[MAX_KEYS] = { 0 };

// MOUSE
static int mouseX = 0, mouseY = 0;
static bool leftClickDown = false, rightClickDown = false;

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

WPARAM MapLeftRightKeys(WPARAM vk, LPARAM lParam)
{
	WPARAM new_vk = vk;
	UINT scancode = (lParam & 0x00ff0000) >> 16;
	int extended = (lParam & 0x01000000) != 0;

	switch (vk) {
	case VK_SHIFT:
		new_vk = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
		break;
	case VK_CONTROL:
		new_vk = extended ? VK_RCONTROL : VK_LCONTROL;
		break;
	case VK_MENU:
		new_vk = extended ? VK_RMENU : VK_LMENU;
		break;
	default:
		// not a key we map from generic to left/right specialized
		//  just return it.
		new_vk = vk;
		break;
	}

	return new_vk;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!pWindow || hwnd != pWindow->pWindowHandle)
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
		// WINDOW
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

		// KEYBOARD
	case WM_KEYDOWN:
	{
		if (wParam < 256)
			mKeyStates[MapLeftRightKeys(wParam, lParam)] = 1;
		break;
	}
	case WM_KEYUP:
	{
		if (wParam < 256)
			mKeyStates[MapLeftRightKeys(wParam, lParam)] = 0;
		break;
	}
	case WM_CHAR:
	{
		if (currentCharCount >= MAX_KEYS)
			ClearKeyCharBuffer();
		mCharBuffer[currentCharCount++] = (char)wParam;
		break;
	}

		// MOUSE
	case WM_MOUSEMOVE:
	{
		mouseX = (int)(short)LOWORD(lParam);
		mouseY = (int)(short)HIWORD(lParam);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		leftClickDown = true;
		break;
	}
	case WM_LBUTTONUP:
	{
		leftClickDown = false;
		break;
	}
	case WM_RBUTTONDOWN:
	{
		rightClickDown = true;
		break;
	}
	case WM_RBUTTONUP:
	{
		rightClickDown = false;
		break;
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
	//wc.style = CS_DBLCLKS;
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

void GetKeyStates(uint16_t* a_KeyStates)
{
	memcpy(a_KeyStates, mKeyStates, sizeof(uint16_t) * MAX_KEYS);
}

void GetKeyCharBuffer(char* a_uBuffer, uint32_t* a_uNoOfKeys)
{
	memcpy(a_uBuffer, mCharBuffer, sizeof(char) * currentCharCount);
	*a_uNoOfKeys = currentCharCount;
}

void ClearKeyCharBuffer()
{
	memset(mCharBuffer, 0, sizeof(char) * currentCharCount);
	currentCharCount = 0;
}

bool IsLeftClick()
{
	return leftClickDown;
}

bool IsRightClick()
{
	return rightClickDown;
}

void GetMouseCoordinates(int* a_pMouseX, int* a_pMouseY)
{
	*a_pMouseX = mouseX;
	*a_pMouseY = mouseY;
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
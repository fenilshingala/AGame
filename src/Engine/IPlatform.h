#pragma once

#include <stdint.h>
#include <limits>

class IApp;
struct Window
{
	uint32_t width;
	uint32_t height;
	bool fullScreen;
	bool minimized;
	bool reset;
	void* pWindowHandle;
	IApp* pApp;

	Window() :
		width(0), height(0), fullScreen(false), minimized(false), reset(true), pWindowHandle(nullptr), pApp(nullptr)
	{}
};

void InitWindow(Window* a_pWindow);
void ExitWindow(Window* a_pWindow);
bool WindowShouldClose();

#if defined(_WIN32)
const int MAX_KEYS = 256;
void GetKeyStates(uint16_t* a_KeyStates);
void GetKeyCharBuffer(char* a_uBuffer, uint32_t* a_uNoOfKeys);
void ClearKeyCharBuffer();

bool IsLeftClick();
bool IsRightClick();
#endif
#if defined(__ANDROID_API__)
const float TOUCH_LOCATION_INVALID = std::numeric_limits<float>::max();
bool IsTouch();
void GetTouchLocation(float* x, float* y);
#endif
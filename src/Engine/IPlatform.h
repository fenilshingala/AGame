#pragma once

#include <stdint.h>

class IApp;
struct Window
{
	uint32_t width;
	uint32_t height;
	bool fullScreen;
	void* pWindowHandle;
	IApp* pApp;

	Window() :
		width(0), height(0), fullScreen(false), pWindowHandle(nullptr), pApp(nullptr)
	{}
};

void InitWindow(Window* a_pWindow);
void ExitWindow(Window* a_pWindow);
bool WindowShouldClose();
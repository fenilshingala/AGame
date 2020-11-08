#include "../../IApp.h"

int WindowsMain(int argc, char** argv, IApp* pApp)
{
	pApp->Init();
	pApp->Update();
	pApp->Exit();

	return 0;
}
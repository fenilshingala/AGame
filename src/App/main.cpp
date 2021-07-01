#include "../Engine/Platform.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/App.h"
#include "../Engine/FrameRateController.h"

#include "../Engine/Log.h"
#include "RenderSystem.h"

class App : public IApp
{
	RenderSystem* pRenderSystem;
	FrameRateController* pFRC;

public:
	App() :
		pRenderSystem(nullptr),
		pFRC(nullptr)
	{}

	void Init()
	{
		pRenderSystem = new RenderSystem();
		pFRC = new FrameRateController(60);
		pRenderSystem->Init(this);
	}

	void Exit()
	{
		pRenderSystem->Exit();
		delete pFRC;
		delete pRenderSystem;
	}

	void Load()
	{
		pRenderSystem->Load();
	}

	void Unload()
	{
		pRenderSystem->Unload();
	}

	void Update()
	{
		while (!WindowShouldClose())
		{
			pFRC->FrameStart();

			pRenderSystem->Update();
			pRenderSystem->DrawScene();

			pFRC->FrameEnd();
		}
	}
};

APP_MAIN(App)
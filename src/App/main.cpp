#include "../Engine/Platform.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/App.h"
#include "../Engine/FrameRateController.h"

#include "../Engine/Log.h"
#include "Serializer.h"

#include <queue>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../include/glm/glm.hpp"

#include "AppRenderer.h"
#include "Systems/ModelRenderSystem.h"
#include "Components/ModelComponent.h"

#include <unordered_map>

const std::string resourcePath = {
#if defined(_WIN32)
	"../../src/App/Resources/"
#endif
#if defined(__ANDROID_API__)
	""
#endif
};

AppRenderer* pAppRenderer = nullptr;
FrameRateController* pFRC = nullptr;
EntityManager* pEntityManager = nullptr;
Serializer* pSerializer = nullptr;
ModelRenderSystem* pModelRenderSystem = nullptr;

AppRenderer* GetAppRenderer()
{
	return pAppRenderer;
}

EntityManager* GetEntityManager()
{
	return pEntityManager;
}

ModelRenderSystem* GetModelRenderSystem()
{
	return pModelRenderSystem;
}

class App : public IApp
{
	uint32_t entityCount;
	Entity* pEntities[32] = { nullptr };
public:
	App() :
		entityCount(0), pEntities()
	{}

	void Init()
	{
		pAppRenderer = new AppRenderer();
		pFRC = new FrameRateController(60);
		pEntityManager = new EntityManager();
		pSerializer = new Serializer();
		pModelRenderSystem = new ModelRenderSystem();
		
		pAppRenderer->Init(this);
		
		InitSerializer(&pSerializer);
		LoadLevel(pSerializer, (resourcePath + "Levels/Sample.json").c_str(), pEntityManager, &entityCount, pEntities);

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
		{
			pComponent->Init();
		}
	}

	void Exit()
	{
		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
		{
			pComponent->Exit();
		}

		ExitSerializer(&pSerializer);
		pAppRenderer->Exit();

		delete pModelRenderSystem;
		delete pSerializer;
		delete pEntityManager;
		delete pFRC;
		delete pAppRenderer;
	}

	void Load()
	{
		pAppRenderer->Load();

		ResourceDescriptor* a_pResourceDescriptor = nullptr;
		pAppRenderer->GetResourceDescriptorByName("PBR", &a_pResourceDescriptor);
		pAppRenderer->AllocateModelMatrices("PBR", 32, a_pResourceDescriptor);

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
		{
			pComponent->Load();
		}
	}

	void Unload()
	{
		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
		{
			pComponent->Unload();
		}

		pAppRenderer->DestroyModelMatrices("PBR");
		pAppRenderer->Unload();
	}

	void Update()
	{
		float dt = pFRC->GetFrameTime();
		pFRC->FrameStart();

		pModelRenderSystem->Update();
		pAppRenderer->Update(dt);
		pAppRenderer->DrawScene();

		pFRC->FrameEnd();
	}
};

APP_MAIN(App)
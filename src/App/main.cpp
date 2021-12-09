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
#include "ResourceLoader.h"
#include "Systems/ModelRenderSystem.h"
#include "Systems/SkyboxRenderSystem.h"
#include "Components/ModelComponent.h"
#include "Components/SkyboxComponent.h"

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
ResourceLoader* pResourceLoader = nullptr;
FrameRateController* pFRC = nullptr;
EntityManager* pEntityManager = nullptr;
Serializer* pSerializer = nullptr;
ModelRenderSystem* pModelRenderSystem = nullptr;
SkyboxRenderSystem* pSkyboxRenderSystem = nullptr;

AppRenderer* GetAppRenderer()
{
	return pAppRenderer;
}

ResourceLoader* GetResourceLoader()
{
	return pResourceLoader;
}

EntityManager* GetEntityManager()
{
	return pEntityManager;
}

ModelRenderSystem* GetModelRenderSystem()
{
	return pModelRenderSystem;
}

SkyboxRenderSystem* GetSkyboxRenderSystem()
{
	return pSkyboxRenderSystem;
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
		pResourceLoader = new ResourceLoader();
		pFRC = new FrameRateController(60);
		pEntityManager = new EntityManager();
		pSerializer = new Serializer();
		pModelRenderSystem = new ModelRenderSystem();
		pSkyboxRenderSystem = new SkyboxRenderSystem();
		
		pAppRenderer->Init(this);
		InitResourceLoader(&pResourceLoader);
		
		InitSerializer(&pSerializer);
		LoadLevel(pSerializer, (resourcePath + "Levels/Sample.json").c_str(), pEntityManager, &entityCount, pEntities);

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
			pComponent->Init();

		std::list<Component*> skyboxComponents = pEntityManager->GetComponents<SkyboxComponent>();
		for (Component* pComponent : skyboxComponents)
			pComponent->Init();
	}

	void Exit()
	{
		std::list<Component*> skyboxComponents = pEntityManager->GetComponents<SkyboxComponent>();
		for (Component* pComponent : skyboxComponents)
			pComponent->Exit();

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
			pComponent->Exit();

		ExitSerializer(&pSerializer);
		ExitResourceLoader(&pResourceLoader);
		pAppRenderer->Exit();

		delete pSkyboxRenderSystem;
		delete pModelRenderSystem;
		delete pSerializer;
		delete pEntityManager;
		delete pFRC;
		delete pResourceLoader;
		delete pAppRenderer;
	}

	void Load()
	{
		pAppRenderer->Load();
		LoadResourceLoader(&pResourceLoader);

		ResourceDescriptor* a_pResourceDescriptor = nullptr;
		pAppRenderer->GetResourceDescriptorByName("PBR", &a_pResourceDescriptor);
		pAppRenderer->AllocateModelMatrices("PBR", 32, a_pResourceDescriptor);

		pAppRenderer->GetResourceDescriptorByName("Skybox", &a_pResourceDescriptor);
		pAppRenderer->AllocateModelMatrices("Skybox", 1, a_pResourceDescriptor);

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
			pComponent->Load();

		std::list<Component*> skyboxComponents = pEntityManager->GetComponents<SkyboxComponent>();
		for (Component* pComponent : skyboxComponents)
			pComponent->Load();
	}

	void Unload()
	{
		std::list<Component*> skyboxComponents = pEntityManager->GetComponents<SkyboxComponent>();
		for (Component* pComponent : skyboxComponents)
			pComponent->Unload();

		std::list<Component*> modelComponents = pEntityManager->GetComponents<ModelComponent>();
		for (Component* pComponent : modelComponents)
			pComponent->Unload();

		UnloadResourceLoader(&pResourceLoader);
		pAppRenderer->DestroyModelMatrices("Skybox");
		pAppRenderer->DestroyModelMatrices("PBR");
		pAppRenderer->Unload();
	}

	void Update()
	{
		float dt = pFRC->GetFrameTime();
		pFRC->FrameStart();

		pSkyboxRenderSystem->Update();
		pModelRenderSystem->Update();
		pAppRenderer->Update(dt);
		pAppRenderer->DrawScene();

		pFRC->FrameEnd();
	}
};

APP_MAIN(App)
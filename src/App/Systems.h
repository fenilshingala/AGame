#pragma once

class AppRenderer;
class ModelRenderSystem;
class SkyboxRenderSystem;
class EntityManager;
struct ResourceLoader;

AppRenderer* GetAppRenderer();
ModelRenderSystem* GetModelRenderSystem();
SkyboxRenderSystem* GetSkyboxRenderSystem();
EntityManager* GetEntityManager();
ResourceLoader* GetResourceLoader();
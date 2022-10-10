#pragma once

class AppRenderer;
class ModelRenderSystem;
class SkyboxRenderSystem;
class Physics;
class MotionSystem;
class EntityManager;
struct ResourceLoader;

AppRenderer* GetAppRenderer();
ModelRenderSystem* GetModelRenderSystem();
SkyboxRenderSystem* GetSkyboxRenderSystem();
EntityManager* GetEntityManager();
ResourceLoader* GetResourceLoader();
Physics* GetPhysics();
MotionSystem* GetMotionSystem();
#include "MotionSystem.h"
#include "../../Engine/ECS/Component.h"
#include "../../Engine/ECS/EntityManager.h"

#include "../Components/ControllerComponent.h"
#include "../Components/PositionComponent.h"
#include "../Components/ColliderComponent.h"

#include "../ResourceLoader.h"

#include "../../Engine/Renderer.h"
#include "../../Engine/ModelLoader.h"
#include "../../Engine/Platform.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"
#include "../../../include/glm/gtc/matrix_transform.hpp"

#include "../Systems.h"
#include <queue>
#include "../AppRenderer.h"
#include <WinUser.h>

MotionSystem::MotionSystem()
{}

MotionSystem::~MotionSystem()
{}

static std::vector<ControllerComponent*> controllerComponents;

void MotionSystem::Update(float dt)
{
	static uint16_t keyStates[MAX_KEYS] = { 0 };
	GetKeyStates(keyStates);
	for (ControllerComponent* pControllerComponent : controllerComponents)
	{
		PositionComponent* pPositionComponent = GetEntityManager()->getEntityByID(pControllerComponent->GetOwnerID())->GetComponent<PositionComponent>();
		ColliderComponent* pColliderComponent = GetEntityManager()->getEntityByID(pControllerComponent->GetOwnerID())->GetComponent<ColliderComponent>();
		if (keyStates[VK_LEFT])
		{
			pPositionComponent->x -= 0.1f;
		}
		if (keyStates[VK_RIGHT])
		{
			pPositionComponent->x += 0.1f;
		}
		pColliderComponent->UpdateScaledCollider();
	}
}

void MotionSystem::AddControllerComponent(ControllerComponent* a_ControllerComponent)
{
	controllerComponents.push_back(a_ControllerComponent);
}

void MotionSystem::RemoveControllerComponent(ControllerComponent* a_pControllerComponent)
{
	std::vector<ControllerComponent*>::const_iterator itr = std::find(controllerComponents.begin(), controllerComponents.end(), a_pControllerComponent);
	if (itr != controllerComponents.end())
	{
		controllerComponents.erase(itr);
	}
}
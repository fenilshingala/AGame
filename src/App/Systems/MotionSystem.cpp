#include "MotionSystem.h"
#include "../../Engine/ECS/Component.h"
#include "../../Engine/ECS/EntityManager.h"

#include "../Components/ControllerComponent.h"
#include "../Components/PositionComponent.h"
#include "../Components/ModelComponent.h"

#include "../ResourceLoader.h"

#include "../../Engine/Renderer.h"
#include "../../Engine/ModelLoader.h"
#include "../../Engine/Platform.h"
#include "../../Engine/OS/Windows/KeyBindigs.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"
#include "../../../include/glm/gtc/matrix_transform.hpp"

#include "../Systems.h"
#include <queue>
#include "../AppRenderer.h"
#include <WinUser.h>

enum class PlayerState
{
	IDLE = 0,
	WALKING,
	RUNNING
};

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
		ModelComponent* pModelComponent = GetEntityManager()->getEntityByID(pControllerComponent->GetOwnerID())->GetComponent<ModelComponent>();
		enum class PlayerState curState = (PlayerState)pModelComponent->currentAnimationIndex;
		enum class PlayerState nextState = curState;

		if (!pControllerComponent->playerControl)
			continue;

		bool running = keyStates[KEY_LSHIFT];
		float multiplier = running ? 1.5f : 1.0f;
		float z = 0.0f, y = 0.0f;
		
		if (keyStates[KEY_W])
			z = 0.1f * multiplier;
		if (keyStates[KEY_S])
			z = -0.1f * multiplier;
		if (keyStates[KEY_A])
			y = -0.1f * multiplier;
		if (keyStates[KEY_D])
			y = 0.1f * multiplier;

		if (0.0f != z || 0.0f != y)
		{
			if (running)
				nextState = PlayerState::RUNNING;
			else
				nextState = PlayerState::WALKING;

			pPositionComponent->z += z;
			pPositionComponent->y += y;
		}

		if (curState != nextState)
			pModelComponent->transitioningAnimationIndex = (int)nextState;
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
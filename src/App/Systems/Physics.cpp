#include <vector>
#include "Physics.h"
#include "../../Engine/ECS/Component.h"

#include "../Components/ColliderComponent.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"

#include "../Systems.h"

Physics::Physics()
{}

Physics::~Physics()
{}

static std::vector<ColliderComponent*> colliderComponents;
static std::vector<std::pair<Collider*, Collider*>> collisions;

void Physics::Update(float dt)
{
	uint32_t noOfColliders = (uint32_t)colliderComponents.size();
	if (noOfColliders < 2)
		return;
	for (uint32_t i = 0; i <= noOfColliders-2; ++i)
	{
		for (uint32_t j = i+1; j < noOfColliders; ++j)
		{
			 Collider* pCollider1 = &colliderComponents[i]->mScaledCollider;
			 Collider* pCollider2 = &colliderComponents[j]->mScaledCollider;
			 float centerapart = std::abs((pCollider1->mCenter[0] - pCollider2->mCenter[0]));
			 float extentsapart = std::abs((pCollider1->mR[0] + pCollider2->mR[0]));
			 if (std::abs(pCollider1->mCenter[0] - pCollider2->mCenter[0]) < (pCollider1->mR[0] + pCollider2->mR[0]))
				 if (std::abs(pCollider1->mCenter[1] - pCollider2->mCenter[1]) < (pCollider1->mR[1] + pCollider2->mR[1]))
					 if (std::abs(pCollider1->mCenter[2] - pCollider2->mCenter[2]) < (pCollider1->mR[2] + pCollider2->mR[2]))
						 collisions.push_back(std::make_pair(pCollider1, pCollider2));
		}
	}
	uint32_t noOfCollisions = (uint32_t)collisions.size();
	for (uint32_t i = 0; i < noOfCollisions; ++i)
	{
		Collider* pCollider1 = collisions[i].first;
		Collider* pCollider2 = collisions[i].second;
		// resolve collisions
	}
	collisions.clear();
}

std::vector<std::pair<Collider*, Collider*>>& Physics::GetCollisions()
{
	return collisions;
}

void Physics::AddColliderComponent(ColliderComponent* a_pColliderComponent)
{
	colliderComponents.push_back(a_pColliderComponent);
}

void Physics::RemoveColliderComponent(ColliderComponent* a_pColliderComponent)
{
	std::vector<ColliderComponent*>::const_iterator itr = std::find(colliderComponents.begin(), colliderComponents.end(), a_pColliderComponent);
	if (itr != colliderComponents.end())
	{
		colliderComponents.erase(itr);
	}
}
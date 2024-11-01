#include "../../Engine/ECS/Component.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"

#include <queue>

#include "ColliderComponent.h"
#include "../Systems.h"
#include "../../Engine/ECS/EntityManager.h"
#include "PositionComponent.h"
#include "ModelComponent.h"
#include "../AppRenderer.h"
#include "../../Engine/ModelLoader.h"
#include "../Systems/Physics.h"

DEFINE_COMPONENT(ColliderComponent)

ColliderComponent::ColliderComponent() :
	mScaledCollider(), mCollider(), modelMatrixIndexInBuffer(-1)
{
}

void ColliderComponent::Init()
{}

void ColliderComponent::Exit()
{}

void ColliderComponent::Load()
{
	AppModel* pModel = GetEntityManager()->getEntityByID(GetOwnerID())->GetComponent<ModelComponent>()->GetModel();
	if (pModel)
	{
		float maxBound[3] = { FLT_MIN, FLT_MIN, FLT_MIN };
		float minBound[3] = { FLT_MAX, FLT_MAX, FLT_MAX };

		for (uint32_t i = 0; i < pModel->pModel->vertexBuffer.size(); ++i)
		{
			glm::vec3 pos = pModel->pModel->vertexBuffer[i].pos;
			// X MAX
			if (maxBound[0] < pos.x)
			{
				maxBound[0] = pos.x;
			}
			// X MIN
			if (minBound[0] > pos.x)
			{
				minBound[0] = pos.x;
			}
			// Y MAX
			if (maxBound[1] < pos.y)
			{
				maxBound[1] = pos.y;
			}
			// Y MIN
			if (minBound[1] > pos.y)
			{
				minBound[1] = pos.y;
			}
			// Z MAX
			if (maxBound[2] < pos.z)
			{
				maxBound[2] = pos.z;
			}
			// Z MIN
			if (minBound[2] > pos.z)
			{
				minBound[2] = pos.z;
			}
		}

		mCollider.mCenter[0] = abs(abs(maxBound[0]) - abs(minBound[0])) / 2.0f;
		mCollider.mCenter[0] *= abs(maxBound[0]) > abs(minBound[0]) ? -1 : 1;
		mCollider.mCenter[1] = abs(abs(maxBound[1]) - abs(minBound[1])) / 2.0f;
		mCollider.mCenter[1] *= abs(maxBound[1]) > abs(minBound[1]) ? -1 : 1;
		mCollider.mCenter[2] = abs(abs(maxBound[2]) - abs(minBound[2])) / 2.0f;
		mCollider.mCenter[2] *= abs(maxBound[2]) > abs(minBound[2]) ? -1 : 1;
		mCollider.mR[0] = (maxBound[0] - minBound[0]) / 2.0f;
		mCollider.mR[1] = (maxBound[1] - minBound[1]) / 2.0f;
		mCollider.mR[2] = (maxBound[2] - minBound[2]) / 2.0f;

		UpdateScaledCollider();
	}

	GetAppRenderer()->GetModelMatrixFreeIndex("DebugDraw", &modelMatrixIndexInBuffer);
	GetPhysics()->AddColliderComponent(this);
}

void ColliderComponent::Unload()
{
	GetPhysics()->RemoveColliderComponent(this);
	GetAppRenderer()->RevokeModelMatrixIndex("DebugDraw", modelMatrixIndexInBuffer);
}

void ColliderComponent::UpdateScaledCollider()
{
	PositionComponent* pPositionComponent = GetEntityManager()->getEntityByID(this->GetOwnerID())->GetComponent<PositionComponent>();
	if (pPositionComponent)
	{
		mScaledCollider.mCenter[0] = pPositionComponent->x + (mCollider.mCenter[0] * pPositionComponent->scaleX);
		mScaledCollider.mCenter[1] = -pPositionComponent->y + (mCollider.mCenter[1] * pPositionComponent->scaleY);
		mScaledCollider.mCenter[2] = pPositionComponent->z + (mCollider.mCenter[2] * pPositionComponent->scaleZ);

		mScaledCollider.mR[0] = mCollider.mR[0] * pPositionComponent->scaleX;
		mScaledCollider.mR[1] = mCollider.mR[1] * pPositionComponent->scaleY;
		mScaledCollider.mR[2] = mCollider.mR[2] * pPositionComponent->scaleZ;
	}
}


START_REGISTRATION(ColliderComponent)


START_REFERENCES(ColliderComponent)
END
#include "../../Engine/ECS/Component.h"

#include "ModelComponent.h"
#include "../Systems.h"
#include <algorithm>
#include <limits>
#include "../../Engine/Renderer.h"
#include "../../Engine/ModelLoader.h"

#include <queue>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"

#include "../AppRenderer.h"
#include "../ResourceLoader.h"
#include "../Systems/ModelRenderSystem.h"

DEFINE_COMPONENT(ModelComponent)

ModelComponent::ModelComponent() :
	modelPath(nullptr), pModel(nullptr), modelMatrixIndexInBuffer(-1), currentAnimationIndex(1), currentAnimationTime(0.0f),
	transitioningAnimationIndex(-1), transitioningAnimationTime(0.0f), transitioningTime(0.0f),
	blendFactor(0.0f)
{
}

void ModelComponent::Init()
{
}

void ModelComponent::Exit()
{
	delete pModel;
}

void ModelComponent::Load()
{
	::GetModel(GetResourceLoader(), modelPath, &pModel);
	GetAppRenderer()->GetModelMatrixFreeIndex("PBR", &modelMatrixIndexInBuffer);
	GetModelRenderSystem()->AddModelComponent(this);
}

void ModelComponent::Unload()
{
	GetModelRenderSystem()->RemoveModelComponent(this);
	GetAppRenderer()->RevokeModelMatrixIndex("PBR", modelMatrixIndexInBuffer);
	pModel = nullptr;
	modelMatrixIndexInBuffer = -1;
}

START_REGISTRATION(ModelComponent)

DEFINE_VARIABLE(ModelComponent, modelPath)
DEFINE_VARIABLE(ModelComponent, currentAnimationIndex)

START_REFERENCES(ModelComponent)
REFERENCE_STRING_VARIABLE(ModelComponent, modelPath)
REFERENCE_VARIABLE(ModelComponent, currentAnimationIndex)
END
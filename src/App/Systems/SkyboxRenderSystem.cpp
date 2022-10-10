#include "SkyboxRenderSystem.h"
#include "../../Engine/ECS/Component.h"
#include "../../Engine/ECS/EntityManager.h"

#include "../Components/SkyboxComponent.h"
#include "../Components/PositionComponent.h"

#include "../../Engine/Renderer.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"
#include "../../../include/glm/gtc/matrix_transform.hpp"

#include "../Systems.h"
#include <queue>
#include "../AppRenderer.h"
#include "../ResourceLoader.h"

class SkyboxRenderable : public Renderable
{
public:
	void SetModelMatrixIndex(uint32_t a_ModelMatrixIndex) { modelMatrixIndex = a_ModelMatrixIndex; }
	void SetSkyboxDescriptorSet(DescriptorSet* a_pSkyboxDescriptorSet) { pSkyboxDescriptorSet = a_pSkyboxDescriptorSet; }
	void SetAppMesh(AppMesh* a_pAppMesh) { pAppMesh = a_pAppMesh; }
	void Draw(CommandBuffer* a_pCommandBuffer);

private:
	uint32_t modelMatrixIndex;
	DescriptorSet* pSkyboxDescriptorSet;
	AppMesh* pAppMesh;
};

void SkyboxRenderable::Draw(CommandBuffer* a_pCommandBuffer)
{
	uint32_t w = GetAppRenderer()->GetRenderer()->swapchainRenderTargets[0]->pTexture->desc.width;
	uint32_t h = GetAppRenderer()->GetRenderer()->swapchainRenderTargets[0]->pTexture->desc.height;
	SetViewport(a_pCommandBuffer, 0.0f, 0.0f, (float)w, (float)h, 1.0f, 1.0f);

	BindPipeline(a_pCommandBuffer, GetAppRenderer()->pSkyboxPipeline);
	
	ResourceDescriptor* pSkyboxResourceDescriptor = nullptr;
	GetAppRenderer()->GetResourceDescriptorByName("Skybox", &pSkyboxResourceDescriptor);
	BindDescriptorSet(a_pCommandBuffer, GetAppRenderer()->GetRenderer()->currentFrame, GetAppRenderer()->pSceneDescriptorSet, pSkyboxResourceDescriptor);
	
	GetAppRenderer()->BindModelMatrixDescriptorSet(a_pCommandBuffer, "Skybox", modelMatrixIndex);
	BindDescriptorSet(a_pCommandBuffer, 0, pSkyboxDescriptorSet);

	BindVertexBuffers(a_pCommandBuffer, 1, &(pAppMesh->pVertexBuffer));
	if (pAppMesh->hasIndices) {
		BindIndexBuffer(a_pCommandBuffer, pAppMesh->pIndexBuffer, VK_INDEX_TYPE_UINT32);
	}

	if (pAppMesh->hasIndices) {
		DrawIndexed(a_pCommandBuffer, pAppMesh->indexCount, 0, 0);
	}
	else {
		::Draw(a_pCommandBuffer, pAppMesh->vertexCount, 0);
	}

	SetViewport(a_pCommandBuffer, 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f);
}

static SkyboxRenderable renderables[32] = {};
static std::vector<SkyboxComponent*> skyboxComponents;

SkyboxRenderSystem::SkyboxRenderSystem()
{}

SkyboxRenderSystem::~SkyboxRenderSystem()
{}

void SkyboxRenderSystem::Update()
{
	static uint32_t renderablesCount = (uint32_t)skyboxComponents.size();
	for (uint32_t i=0; i < renderablesCount; ++i)
	{
		SkyboxComponent* pSkyboxComponent = skyboxComponents[i];
		SkyboxRenderable* pRenderable = &renderables[i];
		
		PositionComponent* pPositionComponent = GetEntityManager()->getEntityByID(pSkyboxComponent->GetOwnerID())->GetComponent<PositionComponent>();
		glm::mat4* modelMatrix = nullptr;
		GetAppRenderer()->GetModelMatrixCpuBufferForIndex("Skybox", pSkyboxComponent->GetModelMatrixIndexInBuffer(), &modelMatrix);
		*modelMatrix = glm::mat4(1.0f);
		*modelMatrix = glm::translate(*modelMatrix, glm::vec3(pPositionComponent->x, pPositionComponent->y, pPositionComponent->z));
		*modelMatrix = glm::rotate(*modelMatrix, pPositionComponent->rotation,
			glm::vec3(pPositionComponent->rotationAxisX, pPositionComponent->rotationAxisY, pPositionComponent->rotationAxisZ));
		*modelMatrix = glm::scale(*modelMatrix, glm::vec3(pPositionComponent->scaleX, pPositionComponent->scaleY, pPositionComponent->scaleZ));
		GetAppRenderer()->UpdateModelMatrixGpuBufferForIndex("Skybox", pSkyboxComponent->GetModelMatrixIndexInBuffer());

		pRenderable->SetSkyboxDescriptorSet(pSkyboxComponent->pSkyboxDescriptorSet);
		AppMesh* pAppMesh = nullptr;
		GetMesh(GetResourceLoader(), MeshType::SKYBOX, &pAppMesh);
		pRenderable->SetAppMesh(pAppMesh);
		pRenderable->SetModelMatrixIndex(pSkyboxComponent->GetModelMatrixIndexInBuffer());
		GetAppRenderer()->PushToRenderQueue(pRenderable);
	}
}

void SkyboxRenderSystem::AddSkyboxComponent(SkyboxComponent* a_pSkyboxComponent)
{
	skyboxComponents.push_back(a_pSkyboxComponent);
}

void SkyboxRenderSystem::RemoveSkyboxComponent(SkyboxComponent* a_pSkyboxComponent)
{
	GetAppRenderer()->RevokeModelMatrixIndex("PBR", a_pSkyboxComponent->GetModelMatrixIndexInBuffer());

	std::vector<SkyboxComponent*>::const_iterator itr = std::find(skyboxComponents.begin(), skyboxComponents.end(), a_pSkyboxComponent);
	if (itr != skyboxComponents.end())
	{
		skyboxComponents.erase(itr);
	}
}
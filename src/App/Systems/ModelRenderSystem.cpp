#include "ModelRenderSystem.h"
#include "../../Engine/ECS/Component.h"
#include "../../Engine/ECS/EntityManager.h"

#include "../Components/ModelComponent.h"
#include "../Components/PositionComponent.h"
#include "../Components/ColliderComponent.h"

#include "../ResourceLoader.h"

#include "../../Engine/Renderer.h"
#include "../../Engine/ModelLoader.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"
#include "../../../include/glm/gtc/matrix_transform.hpp"

#include "../Systems.h"
#include <queue>
#include "../AppRenderer.h"

class ModelRenderable : public Renderable
{
public:
	void SetModel(Model* a_pModel) { pModel = a_pModel; }
	void SetModelMatrixIndex(uint32_t a_ModelMatrixIndex) { modelMatrixIndex = a_ModelMatrixIndex; }
	void SetNodeDescriptorSet(DescriptorSet* a_DescriptorSet) { pNodeDescriptorSet = a_DescriptorSet; }
	void Draw(CommandBuffer* a_pCommandBuffer);

private:
	uint32_t modelMatrixIndex;
	DescriptorSet* pNodeDescriptorSet;
	Model* pModel;
};

void renderNode(CommandBuffer* pCommandBuffer, Node* node, Material::AlphaMode alphaMode)
{
	if (node->mesh) {
		// Bind Mesh's descriptor set
		BindDescriptorSet(pCommandBuffer, node->mesh->indexInDescriptorSet, node->mesh->descriptorSet);

		// Render mesh primitives
		for (Primitive* primitive : node->mesh->primitives) {
			if (primitive->material.alphaMode == alphaMode) {

				BindDescriptorSet(pCommandBuffer, primitive->material.indexInDescriptorSet, primitive->material.descriptorSet);

				// Pass material parameters as push constants
				PushConstBlockMaterial pushConstBlockMaterial{};
				pushConstBlockMaterial.emissiveFactor = primitive->material.emissiveFactor;
				// To save push constant space, availabilty and texture coordiante set are combined
				// -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
				pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
				pushConstBlockMaterial.normalTextureSet = primitive->material.normalTexture != nullptr ? primitive->material.texCoordSets.normal : -1;
				pushConstBlockMaterial.occlusionTextureSet = primitive->material.occlusionTexture != nullptr ? primitive->material.texCoordSets.occlusion : -1;
				pushConstBlockMaterial.emissiveTextureSet = primitive->material.emissiveTexture != nullptr ? primitive->material.texCoordSets.emissive : -1;
				pushConstBlockMaterial.alphaMask = static_cast<float>(primitive->material.alphaMode == Material::AlphaMode::ALPHAMODE_MASK);
				pushConstBlockMaterial.alphaMaskCutoff = primitive->material.alphaCutoff;

				// TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

				if (primitive->material.pbrWorkflows.metallicRoughness) {
					// Metallic roughness workflow
					pushConstBlockMaterial.workflow = static_cast<float>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
					pushConstBlockMaterial.baseColorFactor = primitive->material.baseColorFactor;
					pushConstBlockMaterial.metallicFactor = primitive->material.metallicFactor;
					pushConstBlockMaterial.roughnessFactor = primitive->material.roughnessFactor;
					pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive->material.metallicRoughnessTexture != nullptr ? primitive->material.texCoordSets.metallicRoughness : -1;
					pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
				}

				if (primitive->material.pbrWorkflows.specularGlossiness) {
					// Specular glossiness workflow
					pushConstBlockMaterial.workflow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSINESS);
					pushConstBlockMaterial.PhysicalDescriptorTextureSet = primitive->material.extension.specularGlossinessTexture != nullptr ? primitive->material.texCoordSets.specularGlossiness : -1;
					pushConstBlockMaterial.colorTextureSet = primitive->material.extension.diffuseTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
					pushConstBlockMaterial.diffuseFactor = primitive->material.extension.diffuseFactor;
					pushConstBlockMaterial.specularFactor = glm::vec4(primitive->material.extension.specularFactor, 1.0f);
				}

				ResourceDescriptor* pPBRResourceDescriptor = nullptr;
				GetAppRenderer()->GetResourceDescriptorByName("PBR", &pPBRResourceDescriptor);
				BindPushConstants(pCommandBuffer, pPBRResourceDescriptor, "Material", &pushConstBlockMaterial);

				if (primitive->hasIndices) {
					DrawIndexed(pCommandBuffer, primitive->indexCount, primitive->firstIndex, 0);
				}
				else {
					Draw(pCommandBuffer, primitive->vertexCount, 0);
				}
			}
		}

	};
	for (Node* child : node->children) {
		renderNode(pCommandBuffer, child, alphaMode);
	}
}

void ModelRenderable::Draw(CommandBuffer* a_pCommandBuffer)
{
	BindPipeline(a_pCommandBuffer, GetAppRenderer()->pPBRPipeline);
	
	ResourceDescriptor* pPBRResourceDescriptor = nullptr;
	GetAppRenderer()->GetResourceDescriptorByName("PBR", &pPBRResourceDescriptor);
	BindDescriptorSet(a_pCommandBuffer, GetAppRenderer()->GetRenderer()->currentFrame, GetAppRenderer()->pSceneDescriptorSet, pPBRResourceDescriptor);

	GetAppRenderer()->BindModelMatrixDescriptorSet(a_pCommandBuffer, "PBR", modelMatrixIndex);

	BindVertexBuffers(a_pCommandBuffer, 1, &(pModel->vertices));
	if (pModel->indices->buffer != VK_NULL_HANDLE) {
		BindIndexBuffer(a_pCommandBuffer, pModel->indices, VK_INDEX_TYPE_UINT32);
	}

	// Opaque primitives first
	for (Node* node : pModel->nodes) {
		BindDescriptorSet(a_pCommandBuffer, node->index, pNodeDescriptorSet, pPBRResourceDescriptor);
		renderNode(a_pCommandBuffer, node, Material::AlphaMode::ALPHAMODE_OPAQUE);
	}
	// Alpha masked primitives
	for (Node* node : pModel->nodes) {
		BindDescriptorSet(a_pCommandBuffer, node->index, pNodeDescriptorSet, pPBRResourceDescriptor);
		renderNode(a_pCommandBuffer, node, Material::AlphaMode::ALPHAMODE_MASK);
	}
}

static ModelRenderable renderables[32] = {};
static std::vector<ModelComponent*> modelComponents;

class DebugDrawRenderable : public Renderable
{
public:
	void SetModelMatrixIndex(uint32_t a_ModelMatrixIndex) { modelMatrixIndex = a_ModelMatrixIndex; }
	void SetAppMesh(AppMesh* a_pAppMesh) { pAppMesh = a_pAppMesh; }
	void Draw(CommandBuffer* a_pCommandBuffer);

private:
	uint32_t modelMatrixIndex;
	AppMesh* pAppMesh;
};

void DebugDrawRenderable::Draw(CommandBuffer* a_pCommandBuffer)
{
	uint32_t w = GetAppRenderer()->GetRenderer()->swapchainRenderTargets[0]->pTexture->desc.width;
	uint32_t h = GetAppRenderer()->GetRenderer()->swapchainRenderTargets[0]->pTexture->desc.height;
	SetViewport(a_pCommandBuffer, 0.0f, 0.0f, (float)w, (float)h, 0.0f/*1.0f*/, 1.0f);

	BindPipeline(a_pCommandBuffer, GetAppRenderer()->pDebugDrawPipeline);

	ResourceDescriptor* pDebugDrawResourceDescriptor = nullptr;
	GetAppRenderer()->GetResourceDescriptorByName("DebugDraw", &pDebugDrawResourceDescriptor);
	BindDescriptorSet(a_pCommandBuffer, GetAppRenderer()->GetRenderer()->currentFrame, GetAppRenderer()->pSceneDescriptorSet, pDebugDrawResourceDescriptor);

	GetAppRenderer()->BindModelMatrixDescriptorSet(a_pCommandBuffer, "DebugDraw", modelMatrixIndex);

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

	//SetViewport(a_pCommandBuffer, 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f);
}

static DebugDrawRenderable debugDrawRenderables[32] = {};

ModelRenderSystem::ModelRenderSystem()
{}

ModelRenderSystem::~ModelRenderSystem()
{}

void ModelRenderSystem::Update(float dt)
{
	static uint32_t renderablesCount = (uint32_t)modelComponents.size();
	for (uint32_t i=0; i < renderablesCount; ++i)
	{
		ModelComponent* pModelComponent = modelComponents[i];
		
		PositionComponent* pPositionComponent = GetEntityManager()->getEntityByID(pModelComponent->GetOwnerID())->GetComponent<PositionComponent>();
		glm::mat4* modelMatrix = nullptr;
		GetAppRenderer()->GetModelMatrixCpuBufferForIndex("PBR", pModelComponent->GetModelMatrixIndexInBuffer(), &modelMatrix);
		*modelMatrix = glm::mat4(1.0f);
		*modelMatrix = glm::translate(*modelMatrix, glm::vec3(pPositionComponent->x, pPositionComponent->y, pPositionComponent->z));
		*modelMatrix = glm::rotate(*modelMatrix, pPositionComponent->rotation,
			glm::vec3(pPositionComponent->rotationAxisX, pPositionComponent->rotationAxisY, pPositionComponent->rotationAxisZ));
		*modelMatrix = glm::scale(*modelMatrix, glm::vec3(pPositionComponent->scaleX, pPositionComponent->scaleY, pPositionComponent->scaleZ));
		GetAppRenderer()->UpdateModelMatrixGpuBufferForIndex("PBR", pModelComponent->GetModelMatrixIndexInBuffer());

		AppModel* pAppModel = pModelComponent->GetModel();
		if (pAppModel->pModel->animations.size())
		{
			int& curAnimIndex = pModelComponent->currentAnimationIndex;
			int& transAnimIndex = pModelComponent->transitioningAnimationIndex;
			
			const float curAnimLength = pAppModel->pModel->animations[curAnimIndex].end - pAppModel->pModel->animations[curAnimIndex].start;
			float& curAnimTime = pModelComponent->currentAnimationTime;

			if (transAnimIndex == -1)
			{
				UpdateAnimation(curAnimIndex, curAnimTime, pAppModel->pModel);
			}
			else
			{
				const float transAnimLength = pAppModel->pModel->animations[transAnimIndex].end - pAppModel->pModel->animations[transAnimIndex].start;
				float& transAnimTime = pModelComponent->transitioningAnimationTime;
				float& transitionTime = pModelComponent->transitioningTime;
				
				float& blendFactor = pModelComponent->blendFactor;

				const float curNormTime = fmod(curAnimTime / curAnimLength, 1.0f);
				transAnimTime = transAnimLength * curNormTime;

				if (transitionTime <= std::max(curAnimLength, transAnimLength))
				{
					transitionTime += dt;
					blendFactor = transitionTime / std::max(curAnimLength, transAnimLength);
					BlendAnimation(curAnimIndex, transAnimIndex, curAnimTime, transAnimTime, blendFactor, pAppModel->pModel);
				}
				else
				{
					curAnimIndex = transAnimIndex;
					curAnimTime = transAnimTime;
					transAnimIndex = -1;
					transAnimTime = 0.0f;
					transitionTime = 0.0f;
					blendFactor = 0.0f;
				}
			}

			curAnimTime += dt;
			if (curAnimTime > curAnimLength)
				curAnimTime -= curAnimLength;
		}

		ModelRenderable* pRenderable = &renderables[i];
		pRenderable->SetModelMatrixIndex(pModelComponent->GetModelMatrixIndexInBuffer());
		pRenderable->SetNodeDescriptorSet(pModelComponent->GetModel()->pNodeDescriptorSet);
		pRenderable->SetModel(pAppModel->pModel);
		GetAppRenderer()->PushToRenderQueue(pRenderable);

		ColliderComponent* pColliderComponent = GetEntityManager()->getEntityByID(pModelComponent->GetOwnerID())->GetComponent<ColliderComponent>();
		pColliderComponent->UpdateScaledCollider();
		if (pColliderComponent)
		{
			modelMatrix = nullptr;
			GetAppRenderer()->GetModelMatrixCpuBufferForIndex("DebugDraw", pColliderComponent->GetModelMatrixIndexInBuffer(), &modelMatrix);
			*modelMatrix = glm::mat4(1.0f);
			*modelMatrix = glm::translate(*modelMatrix, glm::vec3(pColliderComponent->mScaledCollider.mCenter[0], pColliderComponent->mScaledCollider.mCenter[1], pColliderComponent->mScaledCollider.mCenter[2]));
			*modelMatrix = glm::rotate(*modelMatrix, pPositionComponent->rotation,
				glm::vec3(pPositionComponent->rotationAxisX, pPositionComponent->rotationAxisY, pPositionComponent->rotationAxisZ));
			*modelMatrix = glm::scale(*modelMatrix, glm::vec3(pColliderComponent->mScaledCollider.mR[0], pColliderComponent->mScaledCollider.mR[1], pColliderComponent->mScaledCollider.mR[2]));
			GetAppRenderer()->UpdateModelMatrixGpuBufferForIndex("DebugDraw", pColliderComponent->GetModelMatrixIndexInBuffer());

			DebugDrawRenderable* pDebugDrawRenderable = &debugDrawRenderables[i];
			AppMesh* pAppMesh = nullptr;
			GetMesh(GetResourceLoader(), MeshType::DEBUG_BOX, &pAppMesh);
			pDebugDrawRenderable->SetAppMesh(pAppMesh);
			pDebugDrawRenderable->SetModelMatrixIndex(pColliderComponent->GetModelMatrixIndexInBuffer());
			GetAppRenderer()->PushToRenderQueue(pDebugDrawRenderable);
		}
	}
}

void ModelRenderSystem::AddModelComponent(ModelComponent* a_pModelComponent)
{
	modelComponents.push_back(a_pModelComponent);
}

void ModelRenderSystem::RemoveModelComponent(ModelComponent* a_pModelComponent)
{
	GetAppRenderer()->RevokeModelMatrixIndex("PBR", a_pModelComponent->GetModelMatrixIndexInBuffer());

	std::vector<ModelComponent*>::const_iterator itr = std::find(modelComponents.begin(), modelComponents.end(), a_pModelComponent);
	if (itr != modelComponents.end())
	{
		modelComponents.erase(itr);
	}
}
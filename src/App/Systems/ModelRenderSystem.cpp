#include "ModelRenderSystem.h"
#include "../../Engine/ECS/Component.h"
#include "../../Engine/ECS/EntityManager.h"

#include "../Components/ModelComponent.h"
#include "../Components/PositionComponent.h"

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
	void Draw(CommandBuffer* a_pCommandBuffer);

private:
	uint32_t modelMatrixIndex;
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
	BindVertexBuffers(a_pCommandBuffer, 1, &(pModel->vertices));
	if (pModel->indices->buffer != VK_NULL_HANDLE) {
		BindIndexBuffer(a_pCommandBuffer, pModel->indices, VK_INDEX_TYPE_UINT32);
	}

	GetAppRenderer()->BindModelMatrixDescriptorSet(a_pCommandBuffer, "PBR", modelMatrixIndex);

	// Opaque primitives first
	for (Node* node : pModel->nodes) {
		renderNode(a_pCommandBuffer, node, Material::AlphaMode::ALPHAMODE_OPAQUE);
	}
	// Alpha masked primitives
	for (Node* node : pModel->nodes) {
		renderNode(a_pCommandBuffer, node, Material::AlphaMode::ALPHAMODE_MASK);
	}
}

static ModelRenderable renderables[32] = {};
static std::vector<ModelComponent*> modelComponents;

ModelRenderSystem::ModelRenderSystem()
{}

ModelRenderSystem::~ModelRenderSystem()
{}

void ModelRenderSystem::Update()
{
	static uint32_t renderablesCount = (uint32_t)modelComponents.size();
	for (uint32_t i=0; i < renderablesCount; ++i)
	{
		ModelComponent* pModelComponent = modelComponents[i];
		
		PositionComponent* pPositionComponent = pModelComponent->GetPositionComponent();
		glm::mat4* modelMatrix = nullptr;
		GetAppRenderer()->GetModelMatrixCpuBufferForIndex("PBR", pModelComponent->GetModelMatrixIndexInBuffer(), &modelMatrix);
		*modelMatrix = glm::mat4(1.0f);
		*modelMatrix = glm::translate(*modelMatrix, glm::vec3(pPositionComponent->x, pPositionComponent->y, pPositionComponent->z));
		*modelMatrix = glm::rotate(*modelMatrix, pPositionComponent->rotation,
			glm::vec3(pPositionComponent->rotationAxisX, pPositionComponent->rotationAxisY, pPositionComponent->rotationAxisZ));
		*modelMatrix = glm::scale(*modelMatrix, glm::vec3(pPositionComponent->scaleX, pPositionComponent->scaleY, pPositionComponent->scaleZ));
		GetAppRenderer()->UpdateModelMatrixGpuBufferForIndex("PBR", pModelComponent->GetModelMatrixIndexInBuffer());

		ModelRenderable* pRenderable = &renderables[i];
		pRenderable->SetModelMatrixIndex(pModelComponent->GetModelMatrixIndexInBuffer());
		pRenderable->SetModel(pModelComponent->GetModel()->pModel);
		GetAppRenderer()->PushToRenderQueue(pRenderable);
	}
}

void ModelRenderSystem::AddModelComponent(ModelComponent* a_pModelComponent)
{
	a_pModelComponent->SetPositionComponent( GetEntityManager()->getEntityByID(a_pModelComponent->GetOwnerID())->GetComponent<PositionComponent>() );
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
	a_pModelComponent->SetPositionComponent(nullptr);
}
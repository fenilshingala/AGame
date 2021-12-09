#include <unordered_map>
#include "ResourceLoader.h"
#include "../Engine/Renderer.h"
#include "../Engine/ModelLoader.h"
#include "../Engine/Log.h"
#include <queue>
#include "../App/AppRenderer.h"
#include "../App/Systems.h"

const std::string resourcePath = {
#if defined(_WIN32)
	"../../src/App/Resources/"
#endif
#if defined(__ANDROID_API__)
	""
#endif
};

void DestroyMesh(AppMesh** a_ppAppMesh);

static const float skyBoxVertices[] = {
	10.0f,  -10.0f, -10.0f, 6.0f,    // -z <-> +z
	-10.0f, -10.0f, -10.0f, 6.0f,   -10.0f, 10.0f,  -10.0f, 6.0f,   -10.0f, 10.0f,
	-10.0f, 6.0f,   10.0f,  10.0f,  -10.0f, 6.0f,   10.0f,  -10.0f, -10.0f, 6.0f,

	-10.0f, -10.0f, 10.0f,  2.0f,    //-x
	-10.0f, -10.0f, -10.0f, 2.0f,   -10.0f, 10.0f,  -10.0f, 2.0f,   -10.0f, 10.0f,
	-10.0f, 2.0f,   -10.0f, 10.0f,  10.0f,  2.0f,   -10.0f, -10.0f, 10.0f,  2.0f,

	10.0f,  -10.0f, -10.0f, 1.0f,    //+x
	10.0f,  -10.0f, 10.0f,  1.0f,   10.0f,  10.0f,  10.0f,  1.0f,   10.0f,  10.0f,
	10.0f,  1.0f,   10.0f,  10.0f,  -10.0f, 1.0f,   10.0f,  -10.0f, -10.0f, 1.0f,

	-10.0f, -10.0f, 10.0f,  5.0f,    // +z <-> -z
	-10.0f, 10.0f,  10.0f,  5.0f,   10.0f,  10.0f,  10.0f,  5.0f,   10.0f,  10.0f,
	10.0f,  5.0f,   10.0f,  -10.0f, 10.0f,  5.0f,   -10.0f, -10.0f, 10.0f,  5.0f,

	-10.0f, 10.0f,  -10.0f, 3.0f,    //+y <-> -y
	10.0f,  10.0f,  -10.0f, 3.0f,   10.0f,  10.0f,  10.0f,  3.0f,   10.0f,  10.0f,
	10.0f,  3.0f,   -10.0f, 10.0f,  10.0f,  3.0f,   -10.0f, 10.0f,  -10.0f, 3.0f,

	10.0f,  -10.0f, 10.0f,  4.0f,    //-y <-> +y
	10.0f,  -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f, -10.0f, 4.0f,   -10.0f, -10.0f,
	-10.0f, 4.0f,   -10.0f, -10.0f, 10.0f,  4.0f,   10.0f,  -10.0f, 10.0f,  4.0f,
};

struct ModelMatrixDynamicBuffer
{
	uint32_t		count;
	glm::mat4*		pCpuBuffer;
	Buffer*			pGpuBuffer;
	DescriptorSet*	pDescriptorSet;
	uint8_t*		pOccupiedIndices;
	uint32_t		lastKnownFreeIndex;

	ModelMatrixDynamicBuffer() :
		count(0), pCpuBuffer(nullptr), pGpuBuffer(nullptr), pDescriptorSet(nullptr), pOccupiedIndices(nullptr), lastKnownFreeIndex(0)
	{}
};

void InitResourceLoader(ResourceLoader** a_ppResourceLoader)
{}

void ExitResourceLoader(ResourceLoader** a_ppResourceLoader)
{
	for (std::pair<uint32_t, AppModel*> model : (*a_ppResourceLoader)->modelMap)
	{
		delete model.second->pNodeDescriptorSet;
		delete model.second->pMaterialDescriptorSet;
		delete model.second->pModel;
		delete model.second;
	}
	(*a_ppResourceLoader)->modelMap.clear();

	for (std::pair<uint32_t, ShaderModule*> shader : (*a_ppResourceLoader)->shaderMap)
		delete shader.second;
	(*a_ppResourceLoader)->shaderMap.clear();

	for (std::pair<uint32_t, Texture*> texture : (*a_ppResourceLoader)->textureMap)
		delete texture.second;
	(*a_ppResourceLoader)->textureMap.clear();
}

void LoadResourceLoader(ResourceLoader** a_ppResourceLoader)
{}

void UnloadResourceLoader(ResourceLoader** a_ppResourceLoader)
{
	Renderer* pRenderer = GetAppRenderer()->GetRenderer();
	if (!pRenderer->window.reset)
		return;

	WaitDeviceIdle(pRenderer);
	if (pRenderer->window.reset)
	{
		ResourceLoader* pResourceLoader = *a_ppResourceLoader;
		for (std::pair<uint32_t, AppModel*> model : pResourceLoader->modelMap)
		{
			DestroyModel(model.second->pModel);
			DestroyDescriptorSet(pRenderer, &model.second->pMaterialDescriptorSet);
			DestroyDescriptorSet(pRenderer, &model.second->pNodeDescriptorSet);
		}

		for (std::pair<uint32_t, ShaderModule*> shader : pResourceLoader->shaderMap)
		{
			DestroyShaderModule(pRenderer, &shader.second);
			VkShaderStageFlagBits stage = shader.second->stage;
			shader.second = new(shader.second) ShaderModule();
			shader.second->stage = stage;
		}

		for (std::pair<uint32_t, Texture*> texture : (*a_ppResourceLoader)->textureMap)
			DestroyTexture(pRenderer, &texture.second);

		for (std::pair<uint32_t, AppMesh*> mesh : (*a_ppResourceLoader)->meshMap)
			DestroyMesh(&mesh.second);
	}
}

void updateNodeDescriptor(Renderer* a_pRenderer, Node* node, uint32_t& nodeCounter, DescriptorSet* pNodeDescriptorSet)
{
	if (node->mesh)
	{
		DescriptorUpdateInfo descUpdateInfo = {};
		descUpdateInfo.name = "UBONode";
		descUpdateInfo.mBufferInfo.buffer = node->mesh->uniformBuffer->buffer;
		descUpdateInfo.mBufferInfo.offset = 0;
		descUpdateInfo.mBufferInfo.range = node->mesh->uniformBuffer->desc.bufferSize;

		uint32_t index = nodeCounter++;
		UpdateDescriptorSet(a_pRenderer, index, pNodeDescriptorSet, 1, &descUpdateInfo);

		node->mesh->descriptorSet = pNodeDescriptorSet;
		node->mesh->indexInDescriptorSet = index;
	}

	for (Node* child : node->children)
		updateNodeDescriptor(a_pRenderer, child, nodeCounter, pNodeDescriptorSet);
}

void GetModel(ResourceLoader* a_pResourceLoader, const char* a_sPath, AppModel** a_ppAppModel)
{
	LOG_IF(!(*a_ppAppModel), LogSeverity::ERR, "(*a_ppAppModel) should be nullptr");

	std::unordered_map<uint32_t, AppModel*>& modelMap = a_pResourceLoader->modelMap;
	std::unordered_map<uint32_t, AppModel*>::const_iterator itr = modelMap.find((uint32_t)std::hash<std::string>{}(a_sPath));
	if (itr == modelMap.end())
	{
		*a_ppAppModel = new AppModel();
		Model*& pModel = (*a_ppAppModel)->pModel;
		pModel = new Model();
		DescriptorSet*& pMaterialDescriptorSet = (*a_ppAppModel)->pMaterialDescriptorSet;
		pMaterialDescriptorSet = new DescriptorSet();
		DescriptorSet*& pNodeDescriptorSet = (*a_ppAppModel)->pNodeDescriptorSet;
		pNodeDescriptorSet = new DescriptorSet();

		Renderer* pRenderer = GetAppRenderer()->GetRenderer();
		CreateModelFromFile(pRenderer, resourcePath + a_sPath, pModel);
		modelMap.insert({ (uint32_t)std::hash<std::string>{}(a_sPath), *a_ppAppModel });

		// Set 2
		ResourceDescriptor* pPBRResDesc = nullptr;
		GetAppRenderer()->GetResourceDescriptorByName("PBR", &pPBRResDesc);
		pNodeDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::SET_2, (uint32_t)pModel->linearNodes.size() };
		CreateDescriptorSet(pRenderer, &pNodeDescriptorSet);

		uint32_t nodeCounter = 0;
		for (Node* node : pModel->nodes)
			updateNodeDescriptor(pRenderer, node, nodeCounter, pNodeDescriptorSet);

		// Set 3
		pMaterialDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::SET_3, (uint32_t)pModel->materials.size() };
		CreateDescriptorSet(pRenderer, &pMaterialDescriptorSet);

		const char* samplerNames[5] = {
			"colorMap",
			"physicalDescriptorMap",
			"normalMap",
			"aoMap",
			"emissiveMap"
		};

		{
			DescriptorUpdateInfo descUpdateInfos[5] = {};
			for (uint32_t i = 0; i < 5; ++i)
			{
				descUpdateInfos[i].name = samplerNames[i];
				descUpdateInfos[i].mImageInfo.imageView = pRenderer->defaultResources.defaultImage.imageView;
				descUpdateInfos[i].mImageInfo.imageLayout = pRenderer->defaultResources.defaultImage.desc.initialLayout;
				descUpdateInfos[i].mImageInfo.sampler = pRenderer->defaultResources.defaultSampler.sampler;
			}

			uint32_t matrialIndex = 0;
			for (Material& material : pModel->materials)
			{
				if (material.normalTexture)
				{
					descUpdateInfos[2].mImageInfo.imageView = material.normalTexture->texture->imageView;
					descUpdateInfos[2].mImageInfo.sampler = material.normalTexture->sampler->sampler;
					descUpdateInfos[2].mImageInfo.imageLayout = material.normalTexture->texture->desc.initialLayout;
				}
				if (material.occlusionTexture)
				{
					descUpdateInfos[3].mImageInfo.imageView = material.occlusionTexture->texture->imageView;
					descUpdateInfos[3].mImageInfo.sampler = material.occlusionTexture->sampler->sampler;
					descUpdateInfos[3].mImageInfo.imageLayout = material.occlusionTexture->texture->desc.initialLayout;
				}
				if (material.emissiveTexture)
				{
					descUpdateInfos[4].mImageInfo.imageView = material.emissiveTexture->texture->imageView;
					descUpdateInfos[4].mImageInfo.sampler = material.emissiveTexture->sampler->sampler;
					descUpdateInfos[4].mImageInfo.imageLayout = material.emissiveTexture->texture->desc.initialLayout;
				}

				if (material.pbrWorkflows.metallicRoughness) {
					if (material.baseColorTexture) {
						descUpdateInfos[0].mImageInfo.imageView = material.baseColorTexture->texture->imageView;
						descUpdateInfos[0].mImageInfo.sampler = material.baseColorTexture->sampler->sampler;
						descUpdateInfos[0].mImageInfo.imageLayout = material.baseColorTexture->texture->desc.initialLayout;
					}
					if (material.metallicRoughnessTexture) {
						descUpdateInfos[1].mImageInfo.imageView = material.metallicRoughnessTexture->texture->imageView;
						descUpdateInfos[1].mImageInfo.sampler = material.metallicRoughnessTexture->sampler->sampler;
						descUpdateInfos[1].mImageInfo.imageLayout = material.metallicRoughnessTexture->texture->desc.initialLayout;
					}
				}

				else if (material.pbrWorkflows.specularGlossiness) {
					if (material.extension.diffuseTexture) {
						descUpdateInfos[0].mImageInfo.imageView = material.extension.diffuseTexture->texture->imageView;
						descUpdateInfos[0].mImageInfo.sampler = material.extension.diffuseTexture->sampler->sampler;
						descUpdateInfos[0].mImageInfo.imageLayout = material.extension.diffuseTexture->texture->desc.initialLayout;
					}
					if (material.extension.specularGlossinessTexture) {
						descUpdateInfos[1].mImageInfo.imageView = material.extension.specularGlossinessTexture->texture->imageView;
						descUpdateInfos[1].mImageInfo.sampler = material.extension.specularGlossinessTexture->sampler->sampler;
						descUpdateInfos[1].mImageInfo.imageLayout = material.extension.specularGlossinessTexture->texture->desc.initialLayout;
					}
				}

				uint32_t index = matrialIndex++;
				UpdateDescriptorSet(pRenderer, index, pMaterialDescriptorSet, 5, descUpdateInfos);

				material.descriptorSet = pMaterialDescriptorSet;
				material.indexInDescriptorSet = index;
			}
		}
	}
	else
		*a_ppAppModel = itr->second;
}

void CreateMesh(MeshType e_MeshType, AppMesh** a_ppAppMesh)
{
	AppMesh*& pAppMesh = *a_ppAppMesh;
	pAppMesh = new AppMesh();
	
	switch (e_MeshType)
	{
	case MeshType::SKYBOX:
	{
		Buffer*& vertexBuffer = pAppMesh->pVertexBuffer;
		vertexBuffer = new Buffer();
		vertexBuffer->desc = {
			(uint64_t)sizeof(skyBoxVertices),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(void*)(skyBoxVertices)
		};
		CreateBuffer(GetAppRenderer()->GetRenderer(), &vertexBuffer);
		pAppMesh->vertexCount = (sizeof(skyBoxVertices) / sizeof(skyBoxVertices[0])) / 4;

		break;
	}
	default:
	{
		break;
	}
	}
}

void DestroyMesh(AppMesh** a_ppAppMesh)
{
	Renderer* pRenderer = GetAppRenderer()->GetRenderer();
	DestroyBuffer(pRenderer, &(*a_ppAppMesh)->pVertexBuffer);
	if((*a_ppAppMesh)->hasIndices)
		DestroyBuffer(pRenderer, &(*a_ppAppMesh)->pIndexBuffer);
}

void GetMesh(ResourceLoader* a_pResourceLoader, MeshType e_MeshType, AppMesh** a_ppAppMesh)
{
	std::unordered_map<uint32_t, AppMesh*>::const_iterator itr = a_pResourceLoader->meshMap.find((uint32_t)e_MeshType);

	if (itr == a_pResourceLoader->meshMap.end())
	{
		LOG_IF(!(*a_ppAppMesh), LogSeverity::ERR, "Mesh memory should not be allocated");
		CreateMesh(e_MeshType, a_ppAppMesh);
		a_pResourceLoader->meshMap.insert({ (uint32_t)e_MeshType, *a_ppAppMesh});
	}
	else
	{
		*a_ppAppMesh = itr->second;
	}
}

void GetShaderModule(ResourceLoader* a_pResourceLoader, const char* a_sShaderName, ShaderModule** a_ppShaderModule)
{
	std::unordered_map<uint32_t, ShaderModule*>::const_iterator itr = a_pResourceLoader->shaderMap.find((uint32_t)std::hash<std::string>{}(a_sShaderName));

	if (itr == a_pResourceLoader->shaderMap.end())
	{
		LOG_IF(*a_ppShaderModule, LogSeverity::ERR, "ShaderModule memory not allocated");
		CreateShaderModule(GetAppRenderer()->GetRenderer(), (resourcePath + "Shaders/" + a_sShaderName).c_str(), a_ppShaderModule);
		a_pResourceLoader->shaderMap.insert({ (uint32_t)std::hash<std::string>{}(a_sShaderName), *a_ppShaderModule });
	}
	else
	{
		*a_ppShaderModule = itr->second;
	}
}

void GetTexture(ResourceLoader* a_pResourceLoader, const char* a_sTexturePath, Texture** a_ppTexture)
{
	std::unordered_map<uint32_t, Texture*>::const_iterator itr = a_pResourceLoader->textureMap.find((uint32_t)std::hash<std::string>{}(a_sTexturePath));
	if (itr == a_pResourceLoader->textureMap.end())
	{
		// Load Texture
		Texture* pTexture = new Texture;
		pTexture->desc.filePath = resourcePath + a_sTexturePath;
		pTexture->desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pTexture->desc.format = VK_FORMAT_R8G8B8A8_UNORM;
		pTexture->desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		pTexture->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pTexture->desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		pTexture->desc.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		pTexture->desc.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
		CreateTexture(GetAppRenderer()->GetRenderer(), &pTexture);
		a_pResourceLoader->textureMap.insert({ (uint32_t)std::hash<std::string>{}(a_sTexturePath), pTexture });
		*a_ppTexture = pTexture;
	}
	else
	{
		*a_ppTexture = itr->second;
	}
}
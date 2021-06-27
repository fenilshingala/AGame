#include "../Engine/FileSystem.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/App.h"
#include "../Engine/Renderer.h"
#include "../Engine/ModelLoader.h"
#include "../Engine/Log.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../include/glm/glm.hpp"
#include "../../include/glm/gtc/matrix_transform.hpp"
#include <thread>

ShaderModule* pVertexShader = nullptr;
ShaderModule* pFragmentShader = nullptr;
Buffer* pVertexBuffer = nullptr;
Buffer* pIndexBuffer = nullptr;
Buffer** ppUniformBuffers = nullptr;
Texture* pTexture = nullptr;
Sampler* pSampler = nullptr;
ResourceDescriptor* pResDesc = nullptr;
DescriptorSet* pDescriptorSet = nullptr;
RenderTarget* pDepthBuffer = nullptr;
Pipeline* pPipeline = nullptr;
CommandBuffer** cmdBfrs = nullptr;
static bool appInitialized = false;

Model scene;
ShaderModule* pPBRVertexShader = nullptr;
ShaderModule* pPBRFragmentShader = nullptr;
Buffer** ppSceneBuffers = nullptr;

ResourceDescriptor* pPBRResDesc = nullptr;
DescriptorSet* pSceneDescriptorSet = nullptr;
DescriptorSet* pMaterialDescriptorSet = nullptr;
DescriptorSet* pNodeDescriptorSet = nullptr;
Pipeline* pPBRPipeline = nullptr;

const std::string resourcePath = { 
#if defined(_WIN32)
	"../../src/App/Resources/"
#endif
#if defined(__ANDROID_API__)
	""
#endif
};

struct Vertex {
	glm::vec3 pos;
	glm::vec2 texCoord;
};
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}}
};
const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

struct UniformBufferObject
{
	glm::mat4	model;
	glm::mat4	view;
	glm::mat4	projection;
} ubo;

struct shaderValuesParams {
	glm::vec4 lightDir;
	float exposure = 4.5f;
	float gamma = 2.2f;
	float prefilteredCubeMipLevels;
	float scaleIBLAmbient = 1.0f;
	float debugViewInputs = 0;
	float debugViewEquation = 0;
} shaderValuesParams;

struct PushConstBlockMaterial {
	glm::vec4 baseColorFactor;
	glm::vec4 emissiveFactor;
	glm::vec4 diffuseFactor;
	glm::vec4 specularFactor;
	float workflow;
	int colorTextureSet;
	int PhysicalDescriptorTextureSet;
	int normalTextureSet;
	int occlusionTextureSet;
	int emissiveTextureSet;
	float metallicFactor;
	float roughnessFactor;
	float alphaMask;
	float alphaMaskCutoff;
} pushConstBlockMaterial;

enum PBRWorkflows { PBR_WORKFLOW_METALLIC_ROUGHNESS = 0, PBR_WORKFLOW_SPECULAR_GLOSINESS = 1 };

class App : public IApp
{
	void updateNodeDescriptor(Node* node, uint32_t& nodeCounter)
	{
		if (node->mesh)
		{
			DescriptorUpdateInfo descUpdateInfo = {};
			descUpdateInfo.name = "UBONode";
			descUpdateInfo.mBufferInfo.buffer = node->mesh->uniformBuffer->buffer;
			descUpdateInfo.mBufferInfo.offset = 0;
			descUpdateInfo.mBufferInfo.range = node->mesh->uniformBuffer->desc.bufferSize;

			uint32_t index = nodeCounter++;
			UpdateDescriptorSet(pRenderer, index, pNodeDescriptorSet, 1, &descUpdateInfo);

			node->mesh->descriptorSet = pNodeDescriptorSet;
			node->mesh->indexInDescriptorSet = index;
		}

		for (Node* child : node->children)
			updateNodeDescriptor(child, nodeCounter);
	}

	void renderNode(CommandBuffer* pCommandBuffer, Node* node, Material::AlphaMode alphaMode) {
		if (node->mesh) {
			// Render mesh primitives
			for (Primitive* primitive : node->mesh->primitives) {
				if (primitive->material.alphaMode == alphaMode) {

					BindDescriptorSet(pCommandBuffer, primitive->material.indexInDescriptorSet, primitive->material.descriptorSet);
					BindDescriptorSet(pCommandBuffer, node->mesh->indexInDescriptorSet, node->mesh->descriptorSet);

					// Pass material parameters as push constants
					PushConstBlockMaterial pushConstBlockMaterial{};
					pushConstBlockMaterial.emissiveFactor = primitive->material.emissiveFactor;
					// To save push constant space, availabilty and texture coordiante set are combined
					// -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
					pushConstBlockMaterial.colorTextureSet = primitive->material.baseColorTexture != nullptr ? primitive->material.texCoordSets.baseColor : -1;
					pushConstBlockMaterial.normalTextureSet = primitive->material.normalTexture != nullptr ? primitive->material.texCoordSets.normal : -1;
					pushConstBlockMaterial.occlusionTextureSet = primitive->material.occlusionTexture != nullptr ? primitive->material.texCoordSets.occlusion : -1;
					pushConstBlockMaterial.emissiveTextureSet = primitive->material.emissiveTexture != nullptr ? primitive->material.texCoordSets.emissive : -1;
					pushConstBlockMaterial.alphaMask = static_cast<float>(primitive->material.alphaMode == Material::ALPHAMODE_MASK);
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

					BindPushConstants(pCommandBuffer, pPBRResDesc, "Material", &pushConstBlockMaterial);

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

public:
	App() :
		pRenderer(nullptr)
	{}

	void Init()
	{
		pRenderer = new Renderer();
		pRenderer->window.pApp = this;
		//pRenderer->window.width = 800;
		//pRenderer->window.height = 600;
		pRenderer->maxInFlightFrames = 3;
		
		const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
		cmdBfrs = (CommandBuffer**)malloc(sizeof(CommandBuffer*) * cmdBfrCnt);
		CommandBuffer* cmdBfrPool = (CommandBuffer*)malloc(sizeof(CommandBuffer) * cmdBfrCnt);
		for (uint32_t i = 0; i < cmdBfrCnt; ++i)
		{
			cmdBfrs[i] = cmdBfrPool + i;
			cmdBfrs[i] = new(cmdBfrs[i]) CommandBuffer();
		}

		pVertexShader = new ShaderModule(VK_SHADER_STAGE_VERTEX_BIT);
		pFragmentShader = new ShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT);
		pPBRVertexShader = new ShaderModule(VK_SHADER_STAGE_VERTEX_BIT);
		pPBRFragmentShader = new ShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT);

		pVertexBuffer = new Buffer();
		pIndexBuffer = new Buffer();

		ppUniformBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			ppUniformBuffers[i] = new Buffer();
		
		ppSceneBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			ppSceneBuffers[i] = new Buffer();

		pTexture = new Texture();
		pSampler = new Sampler();

		pResDesc = new ResourceDescriptor(2);
		pDescriptorSet = new DescriptorSet();
		pPBRResDesc = new ResourceDescriptor(11);
		pSceneDescriptorSet = new DescriptorSet();
		pMaterialDescriptorSet = new DescriptorSet();
		pNodeDescriptorSet = new DescriptorSet();

		pPipeline = new Pipeline();
		pPBRPipeline = new Pipeline();
		pDepthBuffer = new RenderTarget();
		pDepthBuffer->pTexture = new Texture();
	}

	void Exit()
	{
		delete pPBRPipeline;
		delete pPipeline;
		delete pDepthBuffer->pTexture;
		delete pDepthBuffer;

		delete pNodeDescriptorSet;
		delete pMaterialDescriptorSet;
		delete pSceneDescriptorSet;
		delete pPBRResDesc;
		delete pDescriptorSet;
		delete pResDesc;

		delete pSampler;
		delete pTexture;
		
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			delete ppSceneBuffers[i];
		free(ppSceneBuffers);
		
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			delete ppUniformBuffers[i];
		free(ppUniformBuffers);

		delete pVertexBuffer;
		delete pIndexBuffer;

		delete pPBRFragmentShader;
		delete pPBRVertexShader;
		delete pFragmentShader;
		delete pVertexShader;

		free(cmdBfrs[0]);
		free(cmdBfrs);

		delete pRenderer;
	}

	void Load()
	{
		if (pRenderer->window.reset)
		{
			InitRenderer(&pRenderer);

			const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
			CreateCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);

			CreateShaderModule(pRenderer, (resourcePath + "Shaders/basic.vert").c_str(), &pVertexShader);
			CreateShaderModule(pRenderer, (resourcePath + "Shaders/basic.frag").c_str(), &pFragmentShader);
			CreateShaderModule(pRenderer, (resourcePath + "Shaders/pbr.vert").c_str(), &pPBRVertexShader);
			CreateShaderModule(pRenderer, (resourcePath + "Shaders/pbr.frag").c_str(), &pPBRFragmentShader);

			pVertexBuffer->desc = {
				(uint64_t)(vertices.size() * sizeof(Vertex)),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				(void*)(vertices.data())
			};
			CreateBuffer(pRenderer, &pVertexBuffer);

			pIndexBuffer->desc = {
				(uint64_t)sizeof(indices),
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				(void*)(indices.data())
			};
			CreateBuffer(pRenderer, &pIndexBuffer);

			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			{
				ppUniformBuffers[i]->desc = {
					(uint64_t)sizeof(UniformBufferObject),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
					(void*)(&ubo)
				};
				CreateBuffer(pRenderer, &ppUniformBuffers[i]);
			}

			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			{
				ppSceneBuffers[i]->desc = {
					(uint64_t)sizeof(shaderValuesParams),
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
					(void*)(&shaderValuesParams)
				};
				CreateBuffer(pRenderer, &ppSceneBuffers[i]);
			}

			pTexture->desc.filePath = resourcePath + "Textures/awesomeface.png";
			pTexture->desc.format = VK_FORMAT_R8G8B8A8_SRGB;
			pTexture->desc.tiling = VK_IMAGE_TILING_OPTIMAL;
			pTexture->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
			pTexture->desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			pTexture->desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			pTexture->desc.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
			pTexture->desc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			CreateTexture(pRenderer, &pTexture);

			pSampler->desc.minFilter = VK_FILTER_LINEAR;
			pSampler->desc.magFilter = VK_FILTER_LINEAR;
			pSampler->desc.mipMapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			pSampler->desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			pSampler->desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			pSampler->desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			CreateSampler(pRenderer, &pSampler);

			pResDesc->desc.descriptors[0] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,	// set
				{
					0,										// binding
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// type
					1,										// count
					VK_SHADER_STAGE_VERTEX_BIT,				// stage flag
					nullptr									// Immutable Sampler
				},
				"UniformBufferObject"
			};
			pResDesc->desc.descriptors[1] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,			// set
				{
					1,												// binding
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,		// type
					1,												// count
					VK_SHADER_STAGE_FRAGMENT_BIT,					// stage flag
					nullptr											// Immutable Sampler
				},
				"texSampler"
			};
			CreateResourceDescriptor(pRenderer, &pResDesc);

			pDescriptorSet->desc = { pResDesc, DescriptorUpdateFrequency::NONE, pRenderer->maxInFlightFrames };
			CreateDescriptorSet(pRenderer, &pDescriptorSet);

			{
				DescriptorUpdateInfo descUpdateInfos[2];
				descUpdateInfos[0].name = "UniformBufferObject";
				descUpdateInfos[0].mBufferInfo.offset = 0;

				descUpdateInfos[1].name = "texSampler";
				descUpdateInfos[1].mImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descUpdateInfos[1].mImageInfo.imageView = pTexture->imageView;
				descUpdateInfos[1].mImageInfo.sampler = pSampler->sampler;

				for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
				{
					descUpdateInfos[0].mBufferInfo.buffer = ppUniformBuffers[i]->buffer;
					descUpdateInfos[0].mBufferInfo.range = ppUniformBuffers[i]->desc.bufferSize;
					UpdateDescriptorSet(pRenderer, i, pDescriptorSet, 2, descUpdateInfos);
				}
			}

			scene.loadFromFile(pRenderer, resourcePath + "Models/pony_cartoon/scene.gltf");

			// Set 0
			pPBRResDesc->desc.descriptors[0] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				"UniformBufferObject"
			};
			pPBRResDesc->desc.descriptors[1] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,
				{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				"UBOParams"
			};
			pPBRResDesc->desc.descriptors[2] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,
				{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				"samplerIrradiance"
			};
			pPBRResDesc->desc.descriptors[3] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,
				{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				"prefilteredMap"
			};
			pPBRResDesc->desc.descriptors[4] =
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				"samplerBRDFLUT"
			};

			// Set 1
			const char* samplerNames[5] = {
				"colorMap",
				"physicalDescriptorMap",
				"normalMap",
				"aoMap",
				"emissiveMap"
			};

			for (uint32_t i = 0; i < 5; ++i)
			{
				pPBRResDesc->desc.descriptors[i+5] =
				{
					(uint32_t)DescriptorUpdateFrequency::PER_FRAME,	// set
					{
						i,											// binding
						VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	// type
						1,											// count
						VK_SHADER_STAGE_FRAGMENT_BIT,				// stage flag
						nullptr										// Immutable Sampler
					},
					samplerNames[i]
				};
			}
			pPBRResDesc->desc.descriptors[10] =
			{
				(uint32_t)DescriptorUpdateFrequency::PER_BATCH,	// set
				{
					0,											// binding
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,			// type
					1,											// count
					VK_SHADER_STAGE_VERTEX_BIT,					// stage flag
					nullptr										// Immutable Sampler
				},
				"UBONode"
			};
			pPBRResDesc->desc.pushConstantCount = 1;
			pPBRResDesc->desc.pushConstants[0] = {
				"Material", {
					VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(PushConstBlockMaterial)
				}
			};

			CreateResourceDescriptor(pRenderer, &pPBRResDesc);

			pSceneDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::NONE, (uint32_t)pRenderer->maxInFlightFrames };
			CreateDescriptorSet(pRenderer, &pSceneDescriptorSet);

			{
				DescriptorUpdateInfo descUpdateInfos[5] = {};
				descUpdateInfos[0].name = "UniformBufferObject";
				descUpdateInfos[0].mBufferInfo.offset = 0;
				descUpdateInfos[1].name = "UBOParams";
				descUpdateInfos[1].mBufferInfo.offset = 0;
				descUpdateInfos[2].name = "samplerIrradiance";
				descUpdateInfos[3].name = "prefilteredMap";
				descUpdateInfos[4].name = "samplerBRDFLUT";
				for (uint32_t i = 2; i < 5; ++i)
				{
					descUpdateInfos[i].mImageInfo.imageLayout = pRenderer->defaultResources.defaultImage.desc.initialLayout;
					descUpdateInfos[i].mImageInfo.imageView = pRenderer->defaultResources.defaultImage.imageView;
					descUpdateInfos[i].mImageInfo.sampler = pSampler->sampler;
				}

				for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
				{
					descUpdateInfos[0].mBufferInfo.buffer = ppUniformBuffers[i]->buffer;
					descUpdateInfos[0].mBufferInfo.range = ppUniformBuffers[i]->desc.bufferSize;
					descUpdateInfos[1].mBufferInfo.buffer = ppSceneBuffers[i]->buffer;
					descUpdateInfos[1].mBufferInfo.range = ppSceneBuffers[i]->desc.bufferSize;
					UpdateDescriptorSet(pRenderer, i, pSceneDescriptorSet, 5, descUpdateInfos);
				}
			}

			pMaterialDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::PER_FRAME, (uint32_t)scene.materials.size() };
			CreateDescriptorSet(pRenderer, &pMaterialDescriptorSet);

			{
				DescriptorUpdateInfo descUpdateInfos[5] = {};
				for (uint32_t i = 0; i < 5; ++i)
				{
					descUpdateInfos[i].name = samplerNames[i];
					descUpdateInfos[i].mImageInfo.imageView = pRenderer->defaultResources.defaultImage.imageView;
					descUpdateInfos[i].mImageInfo.imageLayout = pRenderer->defaultResources.defaultImage.desc.initialLayout;
					descUpdateInfos[i].mImageInfo.sampler = pSampler->sampler;
				}

				uint32_t matrialIndex = 0;
				for (Material& material : scene.materials)
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

			pNodeDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::PER_BATCH, (uint32_t)scene.linearNodes.size() };
			CreateDescriptorSet(pRenderer, &pNodeDescriptorSet);

			uint32_t nodeCounter = 0;
			for (Node* node : scene.nodes)
				updateNodeDescriptor(node, nodeCounter);

			appInitialized = true;
			pRenderer->window.reset = false;
		}

		if (!appInitialized)
			return;

		CreateSwapchain(&pRenderer);

		ShaderModule* shaders[2] = {
			pVertexShader,
			pFragmentShader
		};
		
		pPipeline->desc.shaderCount = 2;
		pPipeline->desc.shaders = shaders;

		VertexAttribute attribs[2];
		attribs[0] = {
			0,								// binding
			sizeof(glm::vec3),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			0,								// location
			VK_FORMAT_R32G32B32_SFLOAT,		// format
			offsetof(Vertex, pos)			// offset
		};
		attribs[1] = {
			0,								// binding
			sizeof(glm::vec2),					// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			1,								// location
			VK_FORMAT_R32G32_SFLOAT,		// format
			offsetof(Vertex, texCoord)		// offset
		};

		pDepthBuffer->pTexture->desc.width = pRenderer->window.width;
		pDepthBuffer->pTexture->desc.height = pRenderer->window.height;
		pDepthBuffer->pTexture->desc.format = VK_FORMAT_D32_SFLOAT;
		pDepthBuffer->pTexture->desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		pDepthBuffer->pTexture->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pDepthBuffer->pTexture->desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		pDepthBuffer->pTexture->desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		pDepthBuffer->pTexture->desc.aspectBits = VK_IMAGE_ASPECT_DEPTH_BIT;
		pDepthBuffer->pTexture->desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		CreateRenderTarget(pRenderer, &pDepthBuffer);

		pPipeline->desc.attribCount = 2;
		pPipeline->desc.attribs = attribs;
		pPipeline->desc.colorAttachmentCount = 1;
		pPipeline->desc.colorFormats[0] = pRenderer->swapchainRenderTargets[0]->pTexture->desc.format;
		pPipeline->desc.cullMode = VK_CULL_MODE_FRONT_BIT;
		//pPipeline->desc.depthBias = 0.0f;
		//pPipeline->desc.depthBiasSlope = 0.0f;
		//pPipeline->desc.depthClamp = 0.0f;
		pPipeline->desc.depthFunction = VK_COMPARE_OP_LESS;
		pPipeline->desc.depthStencilFormat = VK_FORMAT_D32_SFLOAT;
		pPipeline->desc.depthTest = true;
		pPipeline->desc.depthWrite = true;
		pPipeline->desc.frontFace = VK_FRONT_FACE_CLOCKWISE;
		//pPipeline->desc.polygonMode;
		pPipeline->desc.pResourceDescriptor = pResDesc;
		pPipeline->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pPipeline->desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		CreateGraphicsPipeline(pRenderer, &pPipeline);


		pPBRPipeline->desc.shaderCount = 2;
		ShaderModule* pbrShaders[2] = {
			pPBRVertexShader,
			pPBRFragmentShader
		};
		pPBRPipeline->desc.shaders = pbrShaders;

		VertexAttribute pbrAttribs[6] = {};
		pbrAttribs[0] = {
			0,								// binding
			sizeof(glm::vec3),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			0,								// location
			VK_FORMAT_R32G32B32_SFLOAT,		// format
			offsetof(Model::Vertex, pos)			// offset
		};
		pbrAttribs[1] = {
			0,								// binding
			sizeof(glm::vec3),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			1,								// location
			VK_FORMAT_R32G32B32_SFLOAT,		// format
			offsetof(Model::Vertex, normal)			// offset
		};
		pbrAttribs[2] = {
			0,								// binding
			sizeof(glm::vec2),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			2,								// location
			VK_FORMAT_R32G32_SFLOAT,		// format
			offsetof(Model::Vertex, uv0)		// offset
		};
		pbrAttribs[3] = {
			0,								// binding
			sizeof(glm::vec2),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			3,								// location
			VK_FORMAT_R32G32_SFLOAT,		// format
			offsetof(Model::Vertex, uv1)		// offset
		};
		pbrAttribs[4] = {
			0,								// binding
			sizeof(glm::vec4),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			4,								// location
			VK_FORMAT_R32G32B32A32_SFLOAT,	// format
			offsetof(Model::Vertex, joint0)			// offset
		};
		pbrAttribs[5] = {
			0,								// binding
			sizeof(glm::vec4),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			5,								// location
			VK_FORMAT_R32G32B32A32_SFLOAT,	// format
			offsetof(Model::Vertex, weight0)			// offset
		};

		pPBRPipeline->desc.attribCount = 6;
		pPBRPipeline->desc.attribs = pbrAttribs;
		pPBRPipeline->desc.colorAttachmentCount = 1;
		pPBRPipeline->desc.colorFormats[0] = pRenderer->swapchainRenderTargets[0]->pTexture->desc.format;
		pPBRPipeline->desc.cullMode = VK_CULL_MODE_FRONT_BIT;
		//pPBRPipeline->desc.depthBias = 0.0f;
		//pPBRPipeline->desc.depthBiasSlope = 0.0f;
		//pPBRPipeline->desc.depthClamp = 0.0f;
		pPBRPipeline->desc.depthFunction = VK_COMPARE_OP_LESS;
		pPBRPipeline->desc.depthStencilFormat = VK_FORMAT_D32_SFLOAT;
		pPBRPipeline->desc.depthTest = true;
		pPBRPipeline->desc.depthWrite = true;
		pPBRPipeline->desc.frontFace = VK_FRONT_FACE_CLOCKWISE;
		//pPBRPipeline->desc.polygonMode;
		pPBRPipeline->desc.pResourceDescriptor = pPBRResDesc;
		pPBRPipeline->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pPBRPipeline->desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		CreateGraphicsPipeline(pRenderer, &pPBRPipeline);
	}

	void Unload()
	{
		if (!appInitialized)
			return;
		
		WaitDeviceIdle(pRenderer);

		DestroyGraphicsPipeline(pRenderer, &pPBRPipeline);
		DestroyGraphicsPipeline(pRenderer, &pPipeline);
		DestroyRenderTarget(pRenderer, &pDepthBuffer);
		DestroySwapchain(&pRenderer);

		pPBRPipeline->desc.colorAttachmentCount = 0;
		pPipeline->desc.colorAttachmentCount = 0;

		if (pRenderer->window.reset)
		{
			scene.destroy();

			DestroyDescriptorSet(pRenderer, &pDescriptorSet);
			DestroyResourceDescriptor(pRenderer, &pResDesc);

			DestroySampler(pRenderer, &pSampler);
			DestroyTexture(pRenderer, &pTexture);

			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
				DestroyBuffer(pRenderer, &ppUniformBuffers[i]);

			DestroyBuffer(pRenderer, &pVertexBuffer);
			DestroyBuffer(pRenderer, &pIndexBuffer);

			DestroyShaderModule(pRenderer, &pFragmentShader);
			DestroyShaderModule(pRenderer, &pVertexShader);

			const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
			DestroyCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);
			
			ExitRenderer(&pRenderer);

			appInitialized = false;
		}
	}

	void Update()
	{
		while (!WindowShouldClose())
		{
			if (pRenderer->window.minimized)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			uint32_t currentFrame = pRenderer->currentFrame;
			
			{
				float aspect = (float)(pRenderer->swapchainRenderTargets[0]->pTexture->desc.width / pRenderer->swapchainRenderTargets[0]->pTexture->desc.height);
				static auto startTime = std::chrono::high_resolution_clock::now();
				auto currentTime = std::chrono::high_resolution_clock::now();
				float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
				
				ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				ubo.projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
				ubo.projection[1][1] *= -1;

				UpdateBuffer(pRenderer, ppUniformBuffers[currentFrame], &ubo, sizeof(UniformBufferObject));
			}

			{
				uint32_t imageIndex = GetNextSwapchainImage(pRenderer);
				if (imageIndex == -1)
				{
					Unload();
					Load();
					continue;
				}

				CommandBuffer* pCmd = cmdBfrs[currentFrame];
				RenderTarget* pRenderTarget = pRenderer->swapchainRenderTargets[imageIndex];

				BeginCommandBuffer(pCmd);
				TransitionImageLayout(pCmd, pRenderTarget->pTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

				VkClearValue clearColor = {};
				clearColor.color.float32[0] = 0.3f;
				clearColor.color.float32[1] = 0.3f;
				clearColor.color.float32[2] = 0.3f;
				clearColor.color.float32[3] = 1.0f;

				LoadActionsDesc loadDesc;
				loadDesc.clearColors[0] = clearColor;
				loadDesc.loadColorActions[0] = VK_ATTACHMENT_LOAD_OP_CLEAR;
				loadDesc.clearDepth.depthStencil.depth = 1.0f;
				loadDesc.clearDepth.depthStencil.stencil = 0;
				loadDesc.loadDepthAction = VK_ATTACHMENT_LOAD_OP_CLEAR;
				BindRenderTargets(pCmd, 1, &pRenderTarget, &loadDesc, pDepthBuffer);

				SetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTarget->pTexture->desc.width, (float)pRenderTarget->pTexture->desc.height, 0.0f, 1.0f);
				SetScissors(pCmd, 0, 0, pRenderTarget->pTexture->desc.width, pRenderTarget->pTexture->desc.height);
				/*BindDescriptorSet(pCmd, currentFrame, pDescriptorSet);
				BindPipeline(pCmd, pPipeline);
				BindVertexBuffers(pCmd, 1, &pVertexBuffer);
				BindIndexBuffer(pCmd, pIndexBuffer, VK_INDEX_TYPE_UINT16);
				DrawIndexed(pCmd, static_cast<uint32_t>(indices.size()), 0, 0);*/

				BindPipeline(pCmd, pPBRPipeline);
				BindVertexBuffers(pCmd, 1, &(scene.vertices));
				if (scene.indices->buffer != VK_NULL_HANDLE) {
					BindIndexBuffer(pCmd, scene.indices, VK_INDEX_TYPE_UINT32);
				}

				BindDescriptorSet(pCmd, pRenderer->currentFrame, pSceneDescriptorSet);
				// Opaque primitives first
				for (Node* node : scene.nodes) {
					renderNode(pCmd, node, Material::ALPHAMODE_OPAQUE);
				}
				// Alpha masked primitives
				for (Node* node : scene.nodes) {
					renderNode(pCmd, node, Material::ALPHAMODE_MASK);
				}

				BindRenderTargets(pCmd, 0, nullptr);
				TransitionImageLayout(pCmd, pRenderTarget->pTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
				EndCommandBuffer(pCmd);

				Submit(pCmd);
				Present(pCmd);
			}
		}
	}

	Renderer* pRenderer;
};

APP_MAIN(App)
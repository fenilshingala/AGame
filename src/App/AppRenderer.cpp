#include <stdint.h>
#include <unordered_map>
#include <queue>

#include "../Engine/Renderer.h"
#include "../Engine/ModelLoader.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../include/glm/glm.hpp"
#include "../../include/glm/gtc/matrix_transform.hpp"

#include "AppRenderer.h"
#include "ResourceLoader.h"
#include "Systems.h"

#include "../Engine/Camera.h"
#include "../Engine/Log.h"

#if defined(_WIN32)
#include "../Engine/OS/Windows/KeyBindigs.h"
#endif

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

static const std::vector<std::pair<const char*, VkShaderStageFlagBits>> shaderList = {
	{ "basic.vert", VK_SHADER_STAGE_VERTEX_BIT }, {"basic.frag", VK_SHADER_STAGE_FRAGMENT_BIT },
	{ "pbr.vert", VK_SHADER_STAGE_VERTEX_BIT }, {"pbr.frag", VK_SHADER_STAGE_FRAGMENT_BIT },
	{ "skybox.vert", VK_SHADER_STAGE_VERTEX_BIT }, {"skybox.frag", VK_SHADER_STAGE_FRAGMENT_BIT }
};

#if defined(_WIN32)
static int lastMouseX = 0;
static int lastMouseY = 0;
static bool firstMouse = true;
#elif defined(__ANDROID_API__)
static float lastTouchX = 0;
static float lastTouchY = 0;
static bool firstTouch = true;
#endif

const std::string resourcePath = {
#if defined(_WIN32)
	"../../src/App/Resources/"
#endif
#if defined(__ANDROID_API__)
	""
#endif
};

//struct Vertex {
//	glm::vec3 pos;
//	glm::vec2 texCoord;
//};
//const std::vector<Vertex> vertices = {
//	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
//	{{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
//	{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
//	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
//
//	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
//	{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
//	{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}},
//	{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}}
//};
//const std::vector<uint16_t> indices = {
//	0, 1, 2, 2, 3, 0,
//	4, 5, 6, 6, 7, 4
//};

struct UniformBufferObject
{
	glm::mat4	view;
	glm::mat4	projection;
} ubo;

//struct shaderValuesParams {
//	glm::vec4 lightDir;
//	float exposure = 4.5f;
//	float gamma = 2.2f;
//	float prefilteredCubeMipLevels;
//	float scaleIBLAmbient = 1.0f;
//	float debugViewInputs = 0;
//	float debugViewEquation = 0;
//} shaderValuesParams;


void AppRenderer::Init(IApp* a_pApp)
{
	pCamera = new Camera();
	pRenderer = new Renderer();
	pRenderer->window.pApp = a_pApp;
	//pRenderer->window.posX = 10;
	//pRenderer->window.posY = 10;
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

	ppSceneUniformBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
	Buffer* pUniformBufferPool = (Buffer*)malloc(sizeof(Buffer) * pRenderer->maxInFlightFrames);
	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
	{
		ppSceneUniformBuffers[i] = pUniformBufferPool + i;
		ppSceneUniformBuffers[i] = new(ppSceneUniformBuffers[i]) Buffer();
	}

	/*ppSceneBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		ppSceneBuffers[i] = new Buffer();*/

	pPBRResDesc = new ResourceDescriptor(8);
	pPBRPipeline = new Pipeline();
	pSkyboxResDesc = new ResourceDescriptor(8);
	pSkyboxPipeline = new Pipeline();

	pSceneDescriptorSet = new DescriptorSet();
	pDepthBuffer = new RenderTarget();
	pDepthBuffer->pTexture = new Texture();
}

void AppRenderer::Exit()
{
	delete pDepthBuffer->pTexture;
	delete pDepthBuffer;
	delete pSceneDescriptorSet;

	delete pSkyboxPipeline;
	delete pSkyboxResDesc;
	delete pPBRPipeline;
	delete pPBRResDesc;

	/*for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		delete ppSceneBuffers[i];
	free(ppSceneBuffers);*/

	free(ppSceneUniformBuffers[0]);
	free(ppSceneUniformBuffers);

	free(cmdBfrs[0]);
	free(cmdBfrs);

	delete pRenderer;
	delete pCamera;
}

void AppRenderer::Load()
{
	if (pRenderer->window.reset)
	{
		InitRenderer(&pRenderer);

		const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
		CreateCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);

		for (std::pair<const char*, VkShaderStageFlagBits> pair : shaderList)
		{
			ShaderModule* pShaderModule = new ShaderModule();
			pShaderModule->stage = pair.second;
			GetShaderModule(GetResourceLoader(), pair.first, &pShaderModule);
		}

		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			ppSceneUniformBuffers[i]->desc = {
				(uint64_t)sizeof(UniformBufferObject),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				(void*)(&ubo)
			};
			CreateBuffer(pRenderer, &ppSceneUniformBuffers[i]);
		}

		/* ----------------------------------- PBR Resource Desc ----------------------------------- */
		/*for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			ppSceneBuffers[i]->desc = {
				(uint64_t)sizeof(shaderValuesParams),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				(void*)(&shaderValuesParams)
			};
			CreateBuffer(pRenderer, &ppSceneBuffers[i]);
		}*/

		// Set 0
		pPBRResDesc->desc.descriptors[0] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_0,
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			"UniformBufferObject"
		};
		/*pPBRResDesc->desc.descriptors[1] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_0,
			{ 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			"UBOParams"
		};
		pPBRResDesc->desc.descriptors[2] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_0,
			{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			"samplerIrradiance"
		};
		pPBRResDesc->desc.descriptors[3] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_0,
			{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			"prefilteredMap"
		};
		pPBRResDesc->desc.descriptors[4] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_0,
			{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
			"samplerBRDFLUT"
		};*/

		// Set 1
		pPBRResDesc->desc.descriptors[1] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_1,
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
			"UBOModelMatrix"
		};

		// Set 2
		pPBRResDesc->desc.descriptors[2] =
		{
			(uint32_t)DescriptorUpdateFrequency::SET_2,
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
			"UBONode"
		};

		// Set 3
		const char* pbrSamplerNames[5] = {
			"colorMap",
			"physicalDescriptorMap",
			"normalMap",
			"aoMap",
			"emissiveMap"
		};

		for (uint32_t i = 0; i < 5; ++i)
		{
			pPBRResDesc->desc.descriptors[i + 3] =
			{
				(uint32_t)DescriptorUpdateFrequency::SET_3,	
				{ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				pbrSamplerNames[i]
			};
		}

		pPBRResDesc->desc.pushConstantCount = 1;
		pPBRResDesc->desc.pushConstants[0] = {
			"Material", {
				VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PushConstBlockMaterial)
			}
		};

		CreateResourceDescriptor(pRenderer, &pPBRResDesc);
		resourceDescriptorNameMap.insert({ (uint32_t)std::hash<std::string>{}("PBR"), pPBRResDesc });

		pSceneDescriptorSet->desc = { pPBRResDesc, DescriptorUpdateFrequency::SET_0, (uint32_t)pRenderer->maxInFlightFrames };
		CreateDescriptorSet(pRenderer, &pSceneDescriptorSet);
		/* ----------------------------------------------------------------------------------------- */

		/* ----------------------------------- Skybox Resource Desc ----------------------------------- */
		memcpy(pSkyboxResDesc->desc.descriptors, pPBRResDesc->desc.descriptors, sizeof(pPBRResDesc->desc.descriptors[0]) + sizeof(pPBRResDesc->desc.descriptors[1]));
		const char* skyboxSamplerNames[6] = {
			"rightSampler",
			"leftSampler",
			"topSampler",
			"botSampler",
			"frontSampler",
			"backSampler"
		};
		// Set 2
		for (uint32_t i = 0; i < 6; ++i)
		{
			pSkyboxResDesc->desc.descriptors[i + 2] = {
				(uint32_t)DescriptorUpdateFrequency::SET_2,
				{ i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				skyboxSamplerNames[i]
			};
		}
		CreateResourceDescriptor(pRenderer, &pSkyboxResDesc);
		resourceDescriptorNameMap.insert({ (uint32_t)std::hash<std::string>{}("Skybox"), pSkyboxResDesc });
		/* ----------------------------------------------------------------------------------------- */

		/* ----------------------------------- View Projection Scene Descriptors ----------------------------------- */
		{
			DescriptorUpdateInfo descUpdateInfos[5] = {};
			descUpdateInfos[0].name = "UniformBufferObject";
			descUpdateInfos[0].mBufferInfo.offset = 0;
			/*descUpdateInfos[1].name = "UBOParams";
			descUpdateInfos[1].mBufferInfo.offset = 0;
			descUpdateInfos[2].name = "samplerIrradiance";
			descUpdateInfos[3].name = "prefilteredMap";
			descUpdateInfos[4].name = "samplerBRDFLUT";
			for (uint32_t i = 2; i < 5; ++i)
			{
				descUpdateInfos[i].mImageInfo.imageLayout = pRenderer->defaultResources.defaultImage.desc.initialLayout;
				descUpdateInfos[i].mImageInfo.imageView = pRenderer->defaultResources.defaultImage.imageView;
				descUpdateInfos[i].mImageInfo.sampler = pSampler->sampler;
			}*/

			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			{
				descUpdateInfos[0].mBufferInfo.buffer = ppSceneUniformBuffers[i]->buffer;
				descUpdateInfos[0].mBufferInfo.range = ppSceneUniformBuffers[i]->desc.bufferSize;
				//descUpdateInfos[1].mBufferInfo.buffer = ppSceneBuffers[i]->buffer;
				//descUpdateInfos[1].mBufferInfo.range = ppSceneBuffers[i]->desc.bufferSize;
				UpdateDescriptorSet(pRenderer, i, pSceneDescriptorSet, 1, descUpdateInfos);
			}
		}
		/* ----------------------------------------------------------------------------------------- */

		pCamera->rotation_speed *= 0.25f;
		pCamera->translation_speed *= 0.5f;
		pCamera->type = CameraType::FirstPerson;
		pCamera->SetRotation(glm::vec3(-18.0f, -5.0f, 0.0f));
		pCamera->SetTranslation(glm::vec3(0.0f, 2.0f, -3.7f));

		renderSystemInitialized = true;
		pRenderer->window.reset = false;
	}

	if (!renderSystemInitialized)
		return;

	CreateSwapchain(&pRenderer);

	TextureDesc swapChainDesc = pRenderer->swapchainRenderTargets[0]->pTexture->desc;
	pCamera->SetPerspective(60.0f, (float)swapChainDesc.width / (float)swapChainDesc.height, 0.1f, 512.0f);

	ubo.view = pCamera->matrices.view;
	ubo.projection = pCamera->matrices.perspective;
	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		UpdateBuffer(pRenderer, ppSceneUniformBuffers[i], &ubo, sizeof(UniformBufferObject));

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

	ShaderModule *pPBRVertexShader = nullptr, *pPBRFragmentShader = nullptr;
	GetShaderModule(GetResourceLoader(), "pbr.vert", &pPBRVertexShader);
	GetShaderModule(GetResourceLoader(), "pbr.frag", &pPBRFragmentShader);

	pPBRPipeline->desc.shaderCount = 2;
	ShaderModule* pbrShaders[2] = {
		pPBRVertexShader,
		pPBRFragmentShader
	};
	pPBRPipeline->desc.shaders = pbrShaders;

	VertexAttribute pbrAttribs[6] = {};
	pbrAttribs[0] = {
		0,									// binding
		sizeof(glm::vec3),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		0,									// location
		VK_FORMAT_R32G32B32_SFLOAT,			// format
		offsetof(Model::Vertex, pos)		// offset
	};
	pbrAttribs[1] = {
		0,									// binding
		sizeof(glm::vec3),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		1,									// location
		VK_FORMAT_R32G32B32_SFLOAT,			// format
		offsetof(Model::Vertex, normal)		// offset
	};
	pbrAttribs[2] = {
		0,									// binding
		sizeof(glm::vec2),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		2,									// location
		VK_FORMAT_R32G32_SFLOAT,			// format
		offsetof(Model::Vertex, uv0)		// offset
	};
	pbrAttribs[3] = {
		0,									// binding
		sizeof(glm::vec2),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		3,									// location
		VK_FORMAT_R32G32_SFLOAT,			// format
		offsetof(Model::Vertex, uv1)		// offset
	};
	pbrAttribs[4] = {
		0,									// binding
		sizeof(glm::vec4),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		4,									// location
		VK_FORMAT_R32G32B32A32_SFLOAT,		// format
		offsetof(Model::Vertex, joint0)		// offset
	};
	pbrAttribs[5] = {
		0,									// binding
		sizeof(glm::vec4),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		5,									// location
		VK_FORMAT_R32G32B32A32_SFLOAT,		// format
		offsetof(Model::Vertex, weight0)	// offset
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

	// SKYBOX PIPELINE
	ShaderModule* pSkyboxVertexShader = nullptr, * pSkyboxFragmentShader = nullptr;
	GetShaderModule(GetResourceLoader(), "skybox.vert", &pSkyboxVertexShader);
	GetShaderModule(GetResourceLoader(), "skybox.frag", &pSkyboxFragmentShader);

	pSkyboxPipeline->desc.shaderCount = 2;
	ShaderModule* skyboxShaders[2] = {
		pSkyboxVertexShader,
		pSkyboxFragmentShader
	};
	pSkyboxPipeline->desc.shaders = skyboxShaders;
	
	VertexAttribute skyboxAttribs[1] = {};
	skyboxAttribs[0] = {
		0,									// binding
		sizeof(glm::vec4),					// stride
		VK_VERTEX_INPUT_RATE_VERTEX,		// inputrate
		0,									// location
		VK_FORMAT_R32G32B32A32_SFLOAT,		// format
		0									// offset
	};

	pSkyboxPipeline->desc.attribCount = 1;
	pSkyboxPipeline->desc.attribs = skyboxAttribs;
	pSkyboxPipeline->desc.colorAttachmentCount = 1;
	pSkyboxPipeline->desc.colorFormats[0] = pRenderer->swapchainRenderTargets[0]->pTexture->desc.format;
	pSkyboxPipeline->desc.cullMode = VK_CULL_MODE_NONE;
	//pSkyboxPipeline->desc.depthBias = 0.0f;
	//pSkyboxPipeline->desc.depthBiasSlope = 0.0f;
	//pSkyboxPipeline->desc.depthClamp = 0.0f;
	pSkyboxPipeline->desc.depthFunction = VK_COMPARE_OP_LESS;
	pSkyboxPipeline->desc.depthStencilFormat = VK_FORMAT_D32_SFLOAT;
	pSkyboxPipeline->desc.depthTest = false;
	pSkyboxPipeline->desc.depthWrite = false;
	pSkyboxPipeline->desc.frontFace = VK_FRONT_FACE_CLOCKWISE;
	//pSkyboxPipeline->desc.polygonMode;
	pSkyboxPipeline->desc.pResourceDescriptor = pSkyboxResDesc;
	pSkyboxPipeline->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
	pSkyboxPipeline->desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	CreateGraphicsPipeline(pRenderer, &pSkyboxPipeline);
}

void AppRenderer::Unload()
{
	if (!renderSystemInitialized)
		return;

	WaitDeviceIdle(pRenderer);

	DestroyGraphicsPipeline(pRenderer, &pSkyboxPipeline);
	DestroyGraphicsPipeline(pRenderer, &pPBRPipeline);
	DestroyRenderTarget(pRenderer, &pDepthBuffer);
	DestroySwapchain(&pRenderer);

	pPBRPipeline->desc.colorAttachmentCount = 0;

	if (pRenderer->window.reset)
	{
		DestroyResourceDescriptor(pRenderer, &pSkyboxResDesc);
		DestroyResourceDescriptor(pRenderer, &pPBRResDesc);
		resourceDescriptorNameMap.clear();

		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			DestroyBuffer(pRenderer, &ppSceneUniformBuffers[i]);

		const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
		DestroyCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);

		ExitRenderer(&pRenderer);

		renderSystemInitialized = false;
	}
}

void AppRenderer::Update(float f_dt)
{
	pCamera->keys.up = false;
	pCamera->keys.down = false;
	pCamera->keys.left = false;
	pCamera->keys.right = false;

#if defined(_WIN32)
	// INPUT UPDATE
	uint16_t keyStates[MAX_KEYS];
	GetKeyStates(keyStates);

	// CAMERA UPDATE
	int xpos = 0, ypos = 0;
	GetMouseCoordinates(&xpos, &ypos);
	if (firstMouse)
	{
		lastMouseX = xpos;
		lastMouseY = ypos;
		firstMouse = false;
	}

	int xoffset = xpos - lastMouseX;
	int yoffset = lastMouseY - ypos;

	lastMouseX = xpos;
	lastMouseY = ypos;

	if (IsRightClick())
	{
		if (keyStates[KEY_W])
		{
			pCamera->keys.up = true;
		}
		if (keyStates[KEY_S])
		{
			pCamera->keys.down = true;
		}
		if (keyStates[KEY_A])
		{
			pCamera->keys.left = true;
		}
		if (keyStates[KEY_D])
		{
			pCamera->keys.right = true;
		}

		if(xoffset != 0.0f || yoffset != 0.0f || pCamera->IsMoving())
		{
			pCamera->Rotate(glm::vec3((float)yoffset * 0.5f, (float)xoffset * 0.5f, 0.0f));
			pCamera->Update(f_dt);

			ubo.view = pCamera->matrices.view;
			ubo.projection = pCamera->matrices.perspective;
			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
				UpdateBuffer(pRenderer, ppSceneUniformBuffers[i], &ubo, sizeof(UniformBufferObject));
		}
	}
#elif defined(__ANDROID_API__)

	if (IsTouch())
	{
		float touchX = 0.0f, touchY = 0.0f;
		GetTouchLocation(&touchX, &touchY);

		if (firstTouch)
		{
			lastTouchX = touchX;
			lastTouchY = touchY;
			firstTouch = false;
		}

		if (lastTouchX != touchX || lastTouchY != touchY)
		{
			float xoffset = touchX - lastTouchX;
			float yoffset = lastTouchY - touchY;

			lastTouchX = touchX;
			lastTouchY = touchY;

			pCamera->Rotate(glm::vec3((float)yoffset * 0.5f, (float)xoffset * 0.5f, 0.0f));
			pCamera->Update(0.016f);

			ubo.view = pCamera->matrices.view;
			ubo.projection = pCamera->matrices.perspective;
			for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
				UpdateBuffer(pRenderer, ppSceneUniformBuffers[i], &ubo, sizeof(UniformBufferObject));
		}
	}
	else
		firstTouch = true;
#endif
}

void AppRenderer::DrawScene()
{
	uint32_t imageIndex = GetNextSwapchainImage(pRenderer);
	if (imageIndex == -1)
	{
		Unload();
		Load();
		return;
	}

	uint32_t currentFrame = pRenderer->currentFrame;
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

	while (!renderQueue.empty())
	{
		renderQueue.front()->Draw(pCmd);
		renderQueue.pop();
	}

	BindRenderTargets(pCmd, 0, nullptr);
	TransitionImageLayout(pCmd, pRenderTarget->pTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	EndCommandBuffer(pCmd);

	Submit(pCmd);
	Present(pCmd);
}

void AppRenderer::PushToRenderQueue(Renderable* a_pRenderable)
{
	renderQueue.push(a_pRenderable);
}

void AppRenderer::GetResourceDescriptorByName(const char* a_sName, ResourceDescriptor** a_ppResourceDescriptor)
{
	std::unordered_map<uint32_t, ResourceDescriptor*>::const_iterator itr = resourceDescriptorNameMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr != resourceDescriptorNameMap.end())
		*a_ppResourceDescriptor = itr->second;
}

void AppRenderer::AllocateModelMatrices(const char* a_sName, uint32_t a_uCount, ResourceDescriptor* a_pResourceDescriptor)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr != modelMatrixDynamicBufferMap.end())
		return;

	ModelMatrixDynamicBuffer* pDynamicBuffer = new ModelMatrixDynamicBuffer();
	pDynamicBuffer->count = a_uCount;
	pDynamicBuffer->pOccupiedIndices = (uint8_t*)malloc(sizeof(uint8_t) * a_uCount);
	memset(pDynamicBuffer->pOccupiedIndices, 0, sizeof(uint8_t) * a_uCount);

	glm::mat4*& cpuBuffer = pDynamicBuffer->pCpuBuffer;
	cpuBuffer = new glm::mat4[a_uCount * pRenderer->maxInFlightFrames]();

	Buffer*& gpuBuffer = pDynamicBuffer->pGpuBuffer;
	gpuBuffer = new Buffer();
	gpuBuffer->desc = {
			(uint64_t)(a_uCount * sizeof(glm::mat4) * pRenderer->maxInFlightFrames),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			(void*)(cpuBuffer)
	};
	CreateBuffer(pRenderer, &gpuBuffer);

	DescriptorSet*& pDescriptorSet = pDynamicBuffer->pDescriptorSet;
	pDescriptorSet = new DescriptorSet();
	pDescriptorSet->desc = { a_pResourceDescriptor, DescriptorUpdateFrequency::SET_1, pRenderer->maxInFlightFrames };
	CreateDescriptorSet(pRenderer, &pDescriptorSet);

	{
		DescriptorUpdateInfo descUpdateInfo;
		descUpdateInfo.name = "UBOModelMatrix";

		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			descUpdateInfo.mBufferInfo.offset = i * a_uCount * sizeof(glm::mat4);
			descUpdateInfo.mBufferInfo.buffer = gpuBuffer->buffer;
			descUpdateInfo.mBufferInfo.range = sizeof(glm::mat4);
			UpdateDescriptorSet(pRenderer, i, pDescriptorSet, 1, &descUpdateInfo);
		}
	}

	modelMatrixDynamicBufferMap.insert({ (uint32_t)std::hash<std::string>{}(a_sName), pDynamicBuffer });
}

void AppRenderer::DestroyModelMatrices(const char* a_sName)
{
	if (!pRenderer->window.reset)
		return;

	if (!renderSystemInitialized)
		return;

	WaitDeviceIdle(pRenderer);

	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	ModelMatrixDynamicBuffer* pDynamicBuffer = itr->second;
	DestroyDescriptorSet(pRenderer, &pDynamicBuffer->pDescriptorSet);
	delete pDynamicBuffer->pDescriptorSet;
	DestroyBuffer(pRenderer, &pDynamicBuffer->pGpuBuffer);
	delete pDynamicBuffer->pGpuBuffer;
	delete[] pDynamicBuffer->pCpuBuffer;
	free(pDynamicBuffer->pOccupiedIndices);

	modelMatrixDynamicBufferMap.erase(itr);
	delete pDynamicBuffer;
}

void AppRenderer::GetModelMatrixFreeIndex(const char* a_sName, uint32_t* a_pIndex)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	ModelMatrixDynamicBuffer* pDynamicBuffer = itr->second;
	for (uint32_t i = pDynamicBuffer->lastKnownFreeIndex; i < pDynamicBuffer->count; ++i)
	{
		if (!pDynamicBuffer->pOccupiedIndices[i])
		{
			*a_pIndex = i;
			pDynamicBuffer->pOccupiedIndices[i] = 1;
			pDynamicBuffer->lastKnownFreeIndex = i;
			return;
		}
	}

	for (uint32_t i = 0; i < pDynamicBuffer->count; ++i)
	{
		if (!pDynamicBuffer->pOccupiedIndices[i])
		{
			*a_pIndex = i;
			pDynamicBuffer->lastKnownFreeIndex = i;
			return;
		}
	}

	LOG(LogSeverity::ERR, "All model Matrices are occupied for %s", a_sName);
}

void AppRenderer::RevokeModelMatrixIndex(const char* a_sName, const uint32_t a_pIndex)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	ModelMatrixDynamicBuffer* pDynamicBuffer = itr->second;
	pDynamicBuffer->pOccupiedIndices[a_pIndex] = 0;
	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
	{
		uint32_t offset = i * itr->second->count + a_pIndex;
		pDynamicBuffer->pCpuBuffer[offset] = glm::mat4(0.0f);
	}
}

void AppRenderer::GetModelMatrixCpuBufferForIndex(const char* a_sName, const uint32_t a_pIndex, glm::mat4** a_ppCpuBuffer)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	uint32_t offset = pRenderer->currentFrame * itr->second->count + a_pIndex;
	*a_ppCpuBuffer = &(itr->second->pCpuBuffer[offset]);
}

void AppRenderer::UpdateModelMatrixGpuBufferForIndex(const char* a_sName, const uint32_t a_pIndex)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	uint32_t offset = pRenderer->currentFrame * itr->second->count + a_pIndex;
	UpdateBuffer(pRenderer, itr->second->pGpuBuffer, &itr->second->pCpuBuffer[offset], sizeof(glm::mat4), offset * sizeof(glm::mat4));
}

void AppRenderer::BindModelMatrixDescriptorSet(CommandBuffer* a_pCommandBuffer, const char* a_sName, const uint32_t a_pIndex)
{
	std::unordered_map<uint32_t, ModelMatrixDynamicBuffer*>::const_iterator itr = modelMatrixDynamicBufferMap.find((uint32_t)std::hash<std::string>{}(a_sName));
	if (itr == modelMatrixDynamicBufferMap.end())
		return;

	const uint32_t offsets[1] = { a_pIndex * (uint32_t)sizeof(glm::mat4) };
	BindDescriptorSet(a_pCommandBuffer, pRenderer->currentFrame, itr->second->pDescriptorSet, NULL, 1, offsets);
}
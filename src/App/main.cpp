#include "../Engine/OS/FileSystem.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/IApp.h"
#include "../Engine/IRenderer.h"
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


class App : public IApp
{
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

		pVertexBuffer = new Buffer();
		pIndexBuffer = new Buffer();

		ppUniformBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			ppUniformBuffers[i] = new Buffer();

		pTexture = new Texture();
		pSampler = new Sampler();

		pResDesc = new ResourceDescriptor(2);
		pDescriptorSet = new DescriptorSet();

		pPipeline = new Pipeline();
		pDepthBuffer = new RenderTarget();
		pDepthBuffer->pTexture = new Texture();
	}

	void Exit()
	{
		delete pPipeline;
		delete pDepthBuffer->pTexture;
		delete pDepthBuffer;

		delete pDescriptorSet;
		delete pResDesc;

		delete pSampler;
		delete pTexture;
		
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
			delete ppUniformBuffers[i];
		free(ppUniformBuffers);

		delete pVertexBuffer;
		delete pIndexBuffer;

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
	}

	void Unload()
	{
		if (!appInitialized)
			return;
		
		WaitDeviceIdle(pRenderer);

		DestroyGraphicsPipeline(pRenderer, &pPipeline);
		DestroyRenderTarget(pRenderer, &pDepthBuffer);
		DestroySwapchain(&pRenderer);

		pPipeline->desc.colorAttachmentCount = 0;

		if (pRenderer->window.reset)
		{
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
				BindDescriptorSet(pCmd, currentFrame, pDescriptorSet);
				BindPipeline(pCmd, pPipeline);
				BindVertexBuffers(pCmd, 1, &pVertexBuffer);
				BindIndexBuffer(pCmd, pIndexBuffer, VK_INDEX_TYPE_UINT16);
				DrawIndexed(pCmd, static_cast<uint32_t>(indices.size()), 0, 0);

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
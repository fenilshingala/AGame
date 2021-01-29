#include "../Engine/OS/FileSystem.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/IApp.h"
#include "../Engine/IRenderer.h"
#include "../Engine/Log.h"
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

struct float2
{
	float x;
	float y;

	float2() :
		x(0), y(0)
	{}

	float2(float a, float b) :
		x(a), y(b)
	{}
};

struct float3
{
	float x;
	float y;
	float z;

	float3() :
		x(0), y(0), z(0)
	{}

	float3(float a, float b, float c) :
		x(a), y(b), z(c)
	{}
};

struct Vertex {
	float3 pos;
	float2 texCoord;
};
const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}}
};
const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

static float in_fragColor[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
static int fragColorUpdateIndex = 0;

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
		
		InitRenderer(&pRenderer);
		
		const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
		cmdBfrs = (CommandBuffer**)malloc(sizeof(CommandBuffer*) * cmdBfrCnt);
		CommandBuffer* cmdBfrPool = (CommandBuffer*)malloc(sizeof(CommandBuffer) * cmdBfrCnt);
		for (uint32_t i = 0; i < cmdBfrCnt; ++i)
		{
			cmdBfrs[i] = cmdBfrPool + i;
			cmdBfrs[i] = new(cmdBfrs[i]) CommandBuffer();
		}
		CreateCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);

		pVertexShader = new ShaderModule(VK_SHADER_STAGE_VERTEX_BIT);
		pFragmentShader = new ShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT);
		CreateShaderModule(pRenderer, (resourcePath + "Shaders/basic.vert").c_str(), &pVertexShader);
		CreateShaderModule(pRenderer, (resourcePath + "Shaders/basic.frag").c_str(), &pFragmentShader);

		pVertexBuffer = new Buffer();
		pVertexBuffer->desc = {
			(uint64_t)(vertices.size() * sizeof(Vertex)),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(void*)(vertices.data())
		};
		CreateBuffer(pRenderer, &pVertexBuffer);

		pIndexBuffer = new Buffer();
		pIndexBuffer->desc = {
			(uint64_t)sizeof(indices),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(void*)(indices.data())
		};
		CreateBuffer(pRenderer, &pIndexBuffer);

		ppUniformBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			ppUniformBuffers[i] = new Buffer();
			ppUniformBuffers[i]->desc = {
				(uint64_t)sizeof(in_fragColor),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				(void*)(in_fragColor)
			};
			CreateBuffer(pRenderer, &ppUniformBuffers[i]);
		}

		pTexture = new Texture();
		pTexture->desc.filePath = resourcePath + "Textures/awesomeface.png";
		pTexture->desc.format = VK_FORMAT_R8G8B8A8_SRGB;
		pTexture->desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		pTexture->desc.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		pTexture->desc.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		pTexture->desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		pTexture->desc.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
		CreateTexture(pRenderer, &pTexture);

		pSampler = new Sampler();
		pSampler->desc.minFilter = VK_FILTER_LINEAR;
		pSampler->desc.magFilter = VK_FILTER_LINEAR;
		pSampler->desc.mipMapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		pSampler->desc.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pSampler->desc.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		pSampler->desc.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		CreateSampler(pRenderer, &pSampler);

		pResDesc = new ResourceDescriptor();
		pResDesc->desc.descriptors.push_back(
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
			}
		);
		pResDesc->desc.descriptors.push_back(
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
			}
		);
		CreateResourceDescriptor(pRenderer, &pResDesc);

		pDescriptorSet = new DescriptorSet();
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

		pPipeline = new Pipeline();

		appInitialized = true;
	}

	void Exit()
	{
		delete pPipeline;

		DestroyDescriptorSet(pRenderer, &pDescriptorSet);
		delete pDescriptorSet;

		DestroyResourceDescriptor(pRenderer, &pResDesc);
		delete pResDesc;

		DestroySampler(pRenderer, &pSampler);
		delete pSampler;

		DestroyTexture(pRenderer, &pTexture);
		delete pTexture;

		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			DestroyBuffer(pRenderer, &ppUniformBuffers[i]);
			delete ppUniformBuffers[i];
		}
		free(ppUniformBuffers);

		DestroyBuffer(pRenderer, &pVertexBuffer);
		delete pVertexBuffer;
		
		DestroyBuffer(pRenderer, &pIndexBuffer);
		delete pIndexBuffer;

		DestroyShaderModule(pRenderer, &pFragmentShader);
		delete pFragmentShader;
		DestroyShaderModule(pRenderer, &pVertexShader);
		delete pVertexShader;
		
		const uint32_t cmdBfrCnt = pRenderer->maxInFlightFrames;
		DestroyCommandBuffers(pRenderer, cmdBfrCnt, cmdBfrs);
		free(cmdBfrs[0]);
		free(cmdBfrs);

		ExitRenderer(&pRenderer);
		delete pRenderer;
	}

	void Load()
	{
		if (!appInitialized)
			return;

		CreateSwapchain(&pRenderer);

		std::vector<ShaderModule*> shaders = {
			pVertexShader,
			pFragmentShader
		};
		
		pPipeline->desc.shaders = shaders;

		std::vector<VertexAttribute> attribs;
		attribs.push_back({
			0,								// binding
			sizeof(float3),					// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			0,								// location
			VK_FORMAT_R32G32B32_SFLOAT,		// format
			offsetof(Vertex, pos)			// offset
		});
		attribs.push_back({
			0,								// binding
			sizeof(float2),					// stride
			VK_VERTEX_INPUT_RATE_VERTEX,	// inputrate
			1,								// location
			VK_FORMAT_R32G32_SFLOAT,		// format
			offsetof(Vertex, texCoord)		// offset
		});

		pPipeline->desc.attribs = attribs;
		pPipeline->desc.colorFormats.emplace_back(pRenderer->swapchainRenderTargets[0]->pTexture->desc.format);
		pPipeline->desc.cullMode = VK_CULL_MODE_BACK_BIT;
		//pPipeline->desc.depthBias = 0.0f;
		//pPipeline->desc.depthBiasSlope = 0.0f;
		//pPipeline->desc.depthClamp = 0.0f;
		//pPipeline->desc.depthFunction;
		//pPipeline->desc.depthStencilFormat;
		//pPipeline->desc.depthTest;
		//pPipeline->desc.depthWrite;
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
		DestroySwapchain(&pRenderer);

		pPipeline->desc.colorFormats.clear();
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
				if (in_fragColor[fragColorUpdateIndex] <= 1.0f)
				{
					in_fragColor[fragColorUpdateIndex] += 0.001f;
				}
				else
				{
					in_fragColor[fragColorUpdateIndex] = 0.0f;
					fragColorUpdateIndex = ++fragColorUpdateIndex % 3;
				}
				UpdateBuffer(pRenderer, ppUniformBuffers[currentFrame], in_fragColor, sizeof(in_fragColor));
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

				LoadActionsDesc loadDesc;
				VkClearValue clearColor = {};
				clearColor.color.float32[0] = 0.5f;
				clearColor.color.float32[1] = 0.2f;
				clearColor.color.float32[2] = 0.2f;
				clearColor.color.float32[3] = 1.0f;
				loadDesc.clearColors.emplace_back(clearColor);
				loadDesc.loadColorActions.emplace_back(VK_ATTACHMENT_LOAD_OP_CLEAR);
				BindRenderTargets(pCmd, 1, &pRenderTarget, &loadDesc);

				SetViewport(pCmd, 0.0f, 0.0f, (float)pRenderTarget->pTexture->desc.width, (float)pRenderTarget->pTexture->desc.height, 1.0f, 1.0f);
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
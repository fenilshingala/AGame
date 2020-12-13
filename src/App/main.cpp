#include "../Engine/OS/FileSystem.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/IApp.h"
#include "../Engine/IRenderer.h"
#include "../Engine/Log.h"

ShaderModule* pVertexShader = nullptr;
ShaderModule* pFragmentShader = nullptr;
Buffer** ppUniformBuffers = nullptr;
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

static float in_fragColor[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

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
#if defined(_WIN32)
		pRenderer->maxInFlightFrames = 3;
#endif
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

		ppUniformBuffers = (Buffer**)malloc(pRenderer->maxInFlightFrames * sizeof(Buffer*));
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			ppUniformBuffers[i] = new Buffer();
			ppUniformBuffers[i]->desc = {
				(uint64_t)sizeof(in_fragColor),
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				(void*)(in_fragColor)
			};
			CreateBuffer(pRenderer, &ppUniformBuffers[i]);
		}


		pResDesc = new ResourceDescriptor();
		pResDesc->desc.descriptors.push_back(
			{
				(uint32_t)DescriptorUpdateFrequency::NONE,	// set
				{
					0,										// binding
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,		// type
					1,										// count
					VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,	// stage flag
					nullptr									// Immutable Sampler
				},
				"UniformBufferObject"
			}
		);
		CreateResourceDescriptor(pRenderer, &pResDesc);

		pDescriptorSet = new DescriptorSet();
		pDescriptorSet->desc = { pResDesc, DescriptorUpdateFrequency::NONE, pRenderer->maxInFlightFrames };
		CreateDescriptorSet(pRenderer, &pDescriptorSet);

		DescriptorUpdateInfo descUpdateInfo;
		descUpdateInfo.name = "UniformBufferObject";
		descUpdateInfo.mBufferInfo.offset = 0;
		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			descUpdateInfo.mBufferInfo.buffer = ppUniformBuffers[i]->buffer;
			descUpdateInfo.mBufferInfo.range = ppUniformBuffers[i]->desc.bufferSize;
			UpdateDescriptorSet(pRenderer, i, pDescriptorSet, 1, &descUpdateInfo);
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

		for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; ++i)
		{
			DestroyBuffer(pRenderer, &ppUniformBuffers[i]);
			delete ppUniformBuffers[i];
		}
		free(ppUniformBuffers);

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
		//pPipeline->desc.attribs;
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
			uint32_t imageIndex = GetNextSwapchainImage(pRenderer);
			if (imageIndex == -1)
				continue;

			uint32_t currentFrame = pRenderer->currentFrame;
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
			Draw(pCmd, 3, 0);

			BindRenderTargets(pCmd, 0, nullptr);
			TransitionImageLayout(pCmd, pRenderTarget->pTexture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			EndCommandBuffer(pCmd);

			Submit(pCmd);
			Present(pCmd);
		}
	}

	Renderer* pRenderer;
};

APP_MAIN(App)
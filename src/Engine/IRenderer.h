#pragma once

#include "IPlatform.h"
#include <vector>
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__ANDROID_API__)
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>

struct TextureDesc
{
	uint32_t				width;
	uint32_t				height;
	VkFormat				format;
	VkImageTiling			tiling;
	VkImageUsageFlags		usage;
	VkMemoryPropertyFlags	properties;
	VkImageAspectFlagBits	aspectBits;
	VkSampleCountFlagBits	sampleCount;

	TextureDesc() :
		width(0), height(0), format(VK_FORMAT_UNDEFINED), tiling(VK_IMAGE_TILING_OPTIMAL), usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
		properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), aspectBits(VK_IMAGE_ASPECT_COLOR_BIT), sampleCount(VK_SAMPLE_COUNT_1_BIT)
	{}
};

struct Texture
{
	TextureDesc		desc;
	VkImage			image;
	VkDeviceMemory	imageMemory;
	VkImageView		imageView;

	Texture() :
		desc(), image(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE), imageView(VK_NULL_HANDLE)
	{}
};

struct RenderTarget
{
	Texture* pTexture;
	uint32_t id;

#define INVALID_RT_ID uint32_t(-1)
	RenderTarget() :
		pTexture(nullptr), id(INVALID_RT_ID)
	{}
};

struct RenderPassDesc
{
	std::vector<VkFormat>	colorFormats;
	VkFormat				depthStencilFormat;
	VkSampleCountFlagBits	sampleCount;

	RenderPassDesc() :
		colorFormats(), depthStencilFormat(VK_FORMAT_UNDEFINED), sampleCount(VK_SAMPLE_COUNT_1_BIT)
	{}
};

struct LoadActionsDesc
{
	std::vector<VkClearValue>		clearColors;
	std::vector<VkAttachmentLoadOp>	loadColorActions;
	VkClearValue					clearDepth;
	VkAttachmentLoadOp				loadDepthAction;
	VkAttachmentLoadOp				loadStencilAction;

	LoadActionsDesc() :
		clearColors(), loadColorActions(), clearDepth(), loadDepthAction(VK_ATTACHMENT_LOAD_OP_DONT_CARE), loadStencilAction(VK_ATTACHMENT_LOAD_OP_DONT_CARE)
	{
		clearDepth.color = {};
		clearDepth.depthStencil = {};
	}
};

struct RenderPass
{
	RenderPassDesc	desc;
	VkRenderPass	renderPass;

	RenderPass() :
		desc(), renderPass(VK_NULL_HANDLE)
	{}
};

struct Renderer;
struct CommandBuffer
{
	Renderer*		pRenderer;
	VkCommandBuffer commandBuffer;
	VkRenderPass	activeRenderPass;

	CommandBuffer() :
		pRenderer(nullptr), commandBuffer(VK_NULL_HANDLE), activeRenderPass(VK_NULL_HANDLE)
	{}
};

struct ResourceDescriptorDesc
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	ResourceDescriptorDesc() :
		bindings()
	{}
};

struct ResourceDescriptor
{
	ResourceDescriptorDesc	desc;
	VkDescriptorSetLayout	descriptorSetLayout;
	VkPipelineLayout		pipelineLayout;

	ResourceDescriptor() :
		desc(), descriptorSetLayout(VK_NULL_HANDLE), pipelineLayout(VK_NULL_HANDLE)
	{}
};

struct VertexAttribute
{
	uint32_t             binding;
	uint32_t             stride;
	VkVertexInputRate    inputRate;
	uint32_t			 location;
	VkFormat			 format;
	uint32_t			 offset;

	VertexAttribute() :
		binding(0), stride(0), inputRate(VK_VERTEX_INPUT_RATE_VERTEX), location(0), format(VK_FORMAT_UNDEFINED), offset(0)
	{}
};

struct ShaderModule
{
	VkShaderModule			shaderModule;
	VkShaderStageFlagBits	stage;

	ShaderModule(VkShaderStageFlagBits a_stage) :
		shaderModule(VK_NULL_HANDLE), stage(a_stage)
	{}
};

struct PipelineDesc
{
	std::vector<ShaderModule*>		shaders;
	std::vector<VertexAttribute>	attribs;
	VkPrimitiveTopology				topology;
	VkCullModeFlags					cullMode;
	float							depthBias;
	float							depthBiasSlope;
	bool							depthClamp;
	VkFrontFace						frontFace;
	VkPolygonMode					polygonMode;
	VkSampleCountFlagBits			sampleCount;
	bool							depthTest;
	bool							depthWrite;
	VkCompareOp						depthFunction;
	ResourceDescriptor*				pResourceDescriptor;
	std::vector<VkFormat>			colorFormats;
	VkFormat						depthStencilFormat;

	PipelineDesc() :
		attribs(), topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST), cullMode(VK_CULL_MODE_NONE), depthBias(0.0f), depthBiasSlope(0.0f), depthClamp(false), frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE),
		polygonMode(VK_POLYGON_MODE_FILL), sampleCount(VK_SAMPLE_COUNT_1_BIT), depthTest(false), depthWrite(false), depthFunction(VK_COMPARE_OP_NEVER), pResourceDescriptor(nullptr),
		colorFormats(), depthStencilFormat(VK_FORMAT_UNDEFINED)
	{}
};

struct Pipeline
{
	PipelineDesc	desc;
	VkPipeline		pipeline;

	Pipeline() :
		desc(), pipeline(VK_NULL_HANDLE)
	{}
};

struct Renderer
{
	Window window;

	// Vulkan Objects

	VkInstance					instance;
	VkDebugUtilsMessengerEXT	debugMessenger;
	std::vector<const char*>	validationLayers;
	VkSurfaceKHR				surface;

	// device and queues
	VkPhysicalDevice	physicalDevice;
	VkDevice			device;
	VkQueue				graphicsQueue;
	VkQueue				presentQueue;
	//

	// swapchain
	VkSwapchainKHR swapChain;
	RenderTarget** swapchainRenderTargets;
	uint32_t	   swapchainRenderTargetCount;
	//

	VkCommandPool commandPool;

	// syncronization objects
	std::vector<VkSemaphore>	imageAvailableSemaphores;
	std::vector<VkSemaphore>	renderFinishedSemaphores;
	std::vector<VkFence>		inFlightFences;
	uint32_t					maxInFlightFrames;
	uint32_t					currentFrame;
	uint32_t					imageIndex;

	Renderer() :
		instance(), debugMessenger(), surface(), physicalDevice(), device(), graphicsQueue(), presentQueue(), swapChain(), swapchainRenderTargets(), swapchainRenderTargetCount(0),
		commandPool(), maxInFlightFrames(2), currentFrame(0), imageIndex(0)
	{}
};

void InitRenderer(Renderer** a_ppRenderer);
void ExitRenderer(Renderer** a_ppRenderer);
void CreateSwapchain(Renderer** a_ppRenderer);
void DestroySwapchain(Renderer** a_ppRenderer);
void WaitDeviceIdle(Renderer* a_pRenderer);

void CreateTexture(Renderer* a_pRenderer, Texture** a_ppTexture);
void DestroyTexture(Renderer* a_pRenderer, Texture** a_ppTexture);
void TransitionImageLayout(CommandBuffer* a_pCommandBuffer, Texture* a_pTexture, VkImageLayout oldLayout, VkImageLayout newLayout);

void CreateRenderPass(Renderer* a_pRenderer, LoadActionsDesc* pLoadActions, RenderPass** a_ppRenderPass);
void DestroyRenderPass(Renderer* a_pRenderer, RenderPass** a_ppRenderPass);

void CreateResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor);
void DestroyResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor);

void CreateGraphicsPipeline(Renderer* a_ppRenderer, Pipeline** a_ppPipeline);
void DestroyGraphicsPipeline(Renderer* a_ppRenderer, Pipeline** a_ppPipeline);

void CreateRenderTarget(Renderer* a_pRenderer, RenderTarget** a_ppRenderTarget);
void DestroyRenderTarget(Renderer* a_pRenderer, RenderTarget** a_ppRenderTarget);
void BindRenderTargets(CommandBuffer* a_pCommandBuffer, uint32_t a_uRenderTargetCount, RenderTarget** a_ppRenderTargets, LoadActionsDesc* a_pLoadActions = nullptr, RenderTarget* a_pDepthTarget = nullptr);

void CreateShaderModule(Renderer* a_pRenderer, const char* a_sPath, ShaderModule** a_ppShaderModule);
void DestroyShaderModule(Renderer* a_pRenderer, ShaderModule** a_ppShaderModule);

uint32_t GetNextSwapchainImage(Renderer* a_pRenderer);

void CreateCommandBuffers(Renderer* a_pRenderer, uint32_t a_uiCount, CommandBuffer** a_ppCommandBuffers);
void DestroyCommandBuffers(Renderer* a_pRenderer, uint32_t a_uiCount, CommandBuffer** a_ppCommandBuffers);
void BeginCommandBuffer(CommandBuffer* a_pCommandBuffer);
void EndCommandBuffer(CommandBuffer* a_pCommandBuffer);
void SetViewport(CommandBuffer* a_pCommandBuffer, float a_fX, float a_fY, float a_fWidth, float a_fHeight, float a_fMinDepth, float a_fMaxDepth);
void SetScissors(CommandBuffer* a_pCommandBuffer, uint32_t a_uX, uint32_t a_uY, uint32_t a_uWidth, uint32_t a_uHeight);
void BindPipeline(CommandBuffer* a_pCommandBuffer, Pipeline* a_pPipeline);
void Draw(CommandBuffer* a_pCommandBuffer, uint32_t a_uVertexCount, uint32_t a_uFirstVertex);
void Submit(CommandBuffer* a_pCommandBuffer);
void Present(CommandBuffer* a_pCommandBuffer);
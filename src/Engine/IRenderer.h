#pragma once

#include "IPlatform.h"
#include <vector>
#include <unordered_map>
#include <string>
#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__ANDROID_API__)
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>

#define MALLOC_ZERO(type, ptr, size) \
	type* ptr = (type*)malloc(size); \
	memset(ptr, 0, size)

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
	VkImageLayout			initialLayout;
	std::string				filePath;

	TextureDesc() :
		width(0), height(0), format(VK_FORMAT_UNDEFINED), tiling(VK_IMAGE_TILING_OPTIMAL), usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
		properties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), aspectBits(VK_IMAGE_ASPECT_COLOR_BIT), sampleCount(VK_SAMPLE_COUNT_1_BIT),
		initialLayout(VK_IMAGE_LAYOUT_UNDEFINED), filePath()
	{}
};

struct Texture
{
	TextureDesc				desc;
	VkImage					image;
	VkDeviceMemory			imageMemory;
	VkImageView				imageView;
	
	Texture() :
		desc(), image(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE), imageView(VK_NULL_HANDLE)
	{}
};

struct SamplerDesc
{
	VkFilter				minFilter;
	VkFilter				magFilter;
	VkSamplerMipmapMode		mipMapMode;
	VkSamplerAddressMode	addressModeU;
	VkSamplerAddressMode	addressModeV;
	VkSamplerAddressMode	addressModeW;
	float					mipLoadBias;
	float					maxAnisotropy;
	VkCompareOp				compareOp;

	SamplerDesc() :
		minFilter(VK_FILTER_LINEAR), magFilter(VK_FILTER_LINEAR), mipMapMode(VK_SAMPLER_MIPMAP_MODE_NEAREST),
		addressModeU(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE), addressModeV(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE), addressModeW(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
		mipLoadBias(0.0f), maxAnisotropy(0.0f), compareOp(VK_COMPARE_OP_NEVER)
	{}
};

struct Sampler
{
	SamplerDesc desc;
	VkSampler	sampler;
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

struct BufferDesc
{
	uint64_t				bufferSize;
	VkBufferUsageFlags		bufferUsageFlags;
	VkMemoryPropertyFlags	memoryPropertyFlags;
	void*					pData;

	BufferDesc() :
		bufferSize(), bufferUsageFlags(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), memoryPropertyFlags(), pData(nullptr)
	{}

	BufferDesc(uint64_t a_uBufferSize, VkBufferUsageFlags a_bufferUsageFlags, VkMemoryPropertyFlags a_memoryPropertyFlags, void* a_pData) :
		bufferSize(a_uBufferSize), bufferUsageFlags(a_bufferUsageFlags), memoryPropertyFlags(a_memoryPropertyFlags), pData(a_pData)
	{}
};

struct Buffer
{
	BufferDesc		desc;
	VkBuffer		buffer;
	VkDeviceMemory	bufferMemory;

	Buffer() :
		desc(), buffer(VK_NULL_HANDLE), bufferMemory(VK_NULL_HANDLE)
	{}
};

#define MAX_RENDER_TARGET_ATTACHMENTS 8
struct LoadActionsDesc
{
	VkClearValue					clearColors[MAX_RENDER_TARGET_ATTACHMENTS];
	VkAttachmentLoadOp				loadColorActions[MAX_RENDER_TARGET_ATTACHMENTS];
	VkClearValue					clearDepth;
	VkAttachmentLoadOp				loadDepthAction;
	VkAttachmentLoadOp				loadStencilAction;

	LoadActionsDesc() :
		clearColors(), loadColorActions(), clearDepth(), loadDepthAction(VK_ATTACHMENT_LOAD_OP_DONT_CARE), loadStencilAction(VK_ATTACHMENT_LOAD_OP_DONT_CARE)
	{}
};

struct RenderPassDesc
{
	uint32_t				colorAttachmentCount;
	VkFormat				colorFormats[MAX_RENDER_TARGET_ATTACHMENTS];
	VkFormat				depthStencilFormat;
	VkSampleCountFlagBits	sampleCount;

	RenderPassDesc() :
		colorAttachmentCount(0), colorFormats(), depthStencilFormat(VK_FORMAT_UNDEFINED), sampleCount(VK_SAMPLE_COUNT_1_BIT)
	{}
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

enum class DescriptorUpdateFrequency
{
	NONE = 0,
	PER_FRAME,
	PER_BATCH,
	PER_DRAW,
	COUNT,
};

struct DescriptorInfo
{
	uint32_t						set;
	VkDescriptorSetLayoutBinding	binding;
	std::string						name;

	DescriptorInfo(uint32_t a_uSet, VkDescriptorSetLayoutBinding a_binding, std::string a_sName) :
		set(a_uSet), binding(a_binding), name(a_sName)
	{}
};

struct ResourceDescriptorDesc
{
	uint32_t				descriptorCount;
	DescriptorInfo*			descriptors;

	ResourceDescriptorDesc(uint32_t a_uDescriptorCount) :
		descriptorCount(a_uDescriptorCount), descriptors(nullptr)
	{
		MALLOC_ZERO(DescriptorInfo, ptr, sizeof(DescriptorInfo) * a_uDescriptorCount);
		descriptors = ptr;
	}

	~ResourceDescriptorDesc()
	{
		if (descriptors && descriptorCount > 0)
		{
			free(descriptors);
			descriptors = nullptr;
			descriptorCount = 0;
		}
	}
};

struct ResourceDescriptor
{
	ResourceDescriptorDesc						desc;
	uint32_t									descriptorCounts[(uint32_t)DescriptorUpdateFrequency::COUNT] = { 0 };
	DescriptorInfo*								descriptorInfos[(uint32_t)DescriptorUpdateFrequency::COUNT] = { 0 };
	std::unordered_map<uint32_t, uint32_t>		nameToDescriptorInfoIndexMap;
	VkDescriptorSetLayout						descriptorSetLayouts[(uint32_t)DescriptorUpdateFrequency::COUNT] = { 0 };
	VkDescriptorUpdateTemplate					descriptorUpdateTemplates[(uint32_t)DescriptorUpdateFrequency::COUNT] = { 0 };
	VkPipelineLayout							pipelineLayout;

	ResourceDescriptor(uint32_t a_uDescriptorCount) :
		desc(a_uDescriptorCount), descriptorCounts(), descriptorInfos(), nameToDescriptorInfoIndexMap(), descriptorSetLayouts(), descriptorUpdateTemplates(), pipelineLayout(VK_NULL_HANDLE)
	{}
};

struct DescriptorSetDesc
{
	ResourceDescriptor*			pResourceDescriptor;
	DescriptorUpdateFrequency	updateFrequency;
	uint32_t					descriptorSetCount;

	DescriptorSetDesc() :
		pResourceDescriptor(nullptr), updateFrequency(DescriptorUpdateFrequency::NONE), descriptorSetCount(0)
	{}

	DescriptorSetDesc(ResourceDescriptor* a_pResourceDescriptor, DescriptorUpdateFrequency a_eUpdateFrequency, uint32_t a_uDescriptorSetCount) :
		pResourceDescriptor(a_pResourceDescriptor), updateFrequency(a_eUpdateFrequency), descriptorSetCount(a_uDescriptorSetCount)
	{}
};

union DescriptorUpdateData
{
	VkDescriptorBufferInfo mBufferInfo;
	VkDescriptorImageInfo  mImageInfo;
	VkBufferView           mBuferView;
};

struct DescriptorSet
{
	DescriptorSetDesc		desc;
	VkDescriptorSet*		descriptorSets;
	DescriptorUpdateData*	updateData;

	DescriptorSet() :
		desc(), descriptorSets(nullptr), updateData(nullptr)
	{}
};

struct DescriptorUpdateInfo
{
	std::string				name;
	VkDescriptorImageInfo	mImageInfo;
	VkDescriptorBufferInfo	mBufferInfo;
	VkBufferView			mBufferView;

	DescriptorUpdateInfo() :
		name(), mImageInfo(), mBufferInfo(), mBufferView()
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

	VertexAttribute(uint32_t a_uBinding, uint32_t a_uStride, VkVertexInputRate a_VertexInputRate, uint32_t a_uLocation, VkFormat a_Format, uint32_t a_uOffset) :
		binding(a_uBinding), stride(a_uStride), inputRate(a_VertexInputRate), location(a_uLocation), format(a_Format), offset(a_uOffset)
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
	uint32_t						shaderCount;
	ShaderModule**					shaders;
	uint32_t						attribCount;
	VertexAttribute*				attribs;
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
	uint32_t						colorAttachmentCount;
	VkFormat						colorFormats[MAX_RENDER_TARGET_ATTACHMENTS];
	VkFormat						depthStencilFormat;

	PipelineDesc() :
		shaderCount(0), shaders(nullptr), attribCount(0), attribs(nullptr), topology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST), cullMode(VK_CULL_MODE_NONE), depthBias(0.0f), depthBiasSlope(0.0f), depthClamp(false), frontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE),
		polygonMode(VK_POLYGON_MODE_FILL), sampleCount(VK_SAMPLE_COUNT_1_BIT), depthTest(false), depthWrite(false), depthFunction(VK_COMPARE_OP_NEVER), pResourceDescriptor(nullptr),
		colorAttachmentCount(0), colorFormats(), depthStencilFormat(VK_FORMAT_UNDEFINED)
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

struct DefaultResources
{
	Buffer	defaultBuffer;
	Texture defaultImage;

	DefaultResources():
		defaultBuffer(), defaultImage()
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

	VkCommandPool		commandPool;
	VkDescriptorPool	descriptorPool;

	DefaultResources	defaultResources;

	// syncronization objects
	VkSemaphore*				imageAvailableSemaphores;
	VkSemaphore*				renderFinishedSemaphores;
	VkFence*					inFlightFences;
	uint32_t					maxInFlightFrames;
	uint32_t					currentFrame;
	uint32_t					imageIndex;

	Renderer() :
		instance(), debugMessenger(), surface(), physicalDevice(), device(), graphicsQueue(), presentQueue(), swapChain(), swapchainRenderTargets(), swapchainRenderTargetCount(0),
		commandPool(), descriptorPool(), maxInFlightFrames(2), currentFrame(0), imageIndex(0)
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

void CreateSampler(Renderer* a_pRenderer, Sampler** a_ppSampler);
void DestroySampler(Renderer* a_pRenderer, Sampler** a_ppSampler);

void CreateBuffer(Renderer* a_pRenderer, Buffer** a_ppBuffer);
void DestroyBuffer(Renderer* a_pRenderer, Buffer** a_ppBuffer);
void UpdateBuffer(Renderer* a_pRenderer, Buffer* a_pBuffer, void* a_pData, uint64_t a_uSize);

void CreateRenderPass(Renderer* a_pRenderer, LoadActionsDesc* pLoadActions, RenderPass** a_ppRenderPass);
void DestroyRenderPass(Renderer* a_pRenderer, RenderPass** a_ppRenderPass);

void CreateResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor);
void DestroyResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor);
void CreateDescriptorSet(Renderer* a_pRenderer, DescriptorSet** a_ppDescriptorSet);
void DestroyDescriptorSet(Renderer* a_pRenderer, DescriptorSet** a_ppDescriptorSet);
void UpdateDescriptorSet(Renderer* a_pRenderer, uint32_t index, DescriptorSet* a_pDescriptorSet, uint32_t a_uCount, const DescriptorUpdateInfo* a_pDescriptorUpdateInfos);

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
void BindDescriptorSet(CommandBuffer* a_pCommandBuffer, uint32_t a_uIndex, DescriptorSet* a_pDescriptorSet);
void BindVertexBuffers(CommandBuffer* a_pCommandBuffer, uint32_t a_uCount, Buffer** a_ppBuffers);
void BindIndexBuffer(CommandBuffer* a_pCommandBuffer, Buffer* a_pBuffer, VkIndexType a_IndexType);
void Draw(CommandBuffer* a_pCommandBuffer, uint32_t a_uVertexCount, uint32_t a_uFirstVertex);
void DrawIndexed(CommandBuffer* a_pCommandBuffer, uint32_t a_uIndicesCount, uint32_t a_uFirstIndex, uint32_t a_uFirstVertex);
void Submit(CommandBuffer* a_pCommandBuffer);
void Present(CommandBuffer* a_pCommandBuffer);
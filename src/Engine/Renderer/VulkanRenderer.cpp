#include "../IRenderer.h"
#include "../IApp.h"
#include "../Log.h"
#include "../OS/FileSystem.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <algorithm>

#if defined(_WIN32)
#include <direct.h>
#define GetCurrentDir _getcwd
#endif
#if defined(__ANDROID_API__)
#include <shaderc/shaderc.hpp>
#endif
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

void CreateInstance(Renderer** a_pRenderer);
void DestroyInstance(Renderer** a_pRenderer);
void PickPhysicalDevice(Renderer** a_pRenderer);
void CreateLogicalDevice(Renderer** a_ppRenderer);

void CreateCommandPool(Renderer** a_ppRenderer);
void CreateDescriptorPool(Renderer** a_ppRenderer);
void CreateSyncObjects(Renderer** a_ppRenderer);

void BeginSingleTimeCommands(Renderer* a_pRenderer, CommandBuffer* a_pCommandBuffer);
void EndSingleTimeCommands(Renderer* a_pRenderer, CommandBuffer* a_pCommandBuffer);

VkImageView CreateImageView(Renderer* pRenderer, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

static std::unordered_map<uint32_t, VkRenderPass>	renderPasses;
static std::unordered_map<uint32_t, VkFramebuffer>	frameBuffers;
static uint32_t RT_IDs = 0;

void InitRenderer(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	InitWindow(&(*a_ppRenderer)->window);
	CreateInstance(a_ppRenderer);
	PickPhysicalDevice(a_ppRenderer);
	CreateLogicalDevice(a_ppRenderer);
	CreateCommandPool(a_ppRenderer);
	CreateDescriptorPool(a_ppRenderer);
	CreateSyncObjects(a_ppRenderer);
}

void ExitRenderer(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	std::unordered_map<uint32_t, VkRenderPass>::iterator rp_itr = renderPasses.begin();
	for(; rp_itr != renderPasses.end(); ++rp_itr)
	{
		vkDestroyRenderPass(pRenderer->device, rp_itr->second, nullptr);
		rp_itr->second = NULL;
	}
	renderPasses.clear();

	std::unordered_map<uint32_t, VkFramebuffer>::iterator fb_itr = frameBuffers.begin();
	for (; fb_itr != frameBuffers.end(); ++fb_itr)
	{
		vkDestroyFramebuffer(pRenderer->device, fb_itr->second, nullptr);
		fb_itr->second = NULL;
	}
	frameBuffers.clear();

	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; i++)
	{
		vkDestroySemaphore(pRenderer->device, pRenderer->renderFinishedSemaphores[i], nullptr);
		pRenderer->renderFinishedSemaphores[i] = VK_NULL_HANDLE;
		vkDestroySemaphore(pRenderer->device, pRenderer->imageAvailableSemaphores[i], nullptr);
		pRenderer->imageAvailableSemaphores[i] = VK_NULL_HANDLE;
		vkDestroyFence(pRenderer->device, pRenderer->inFlightFences[i], nullptr);
		pRenderer->inFlightFences[i] = VK_NULL_HANDLE;
	}
	vkDestroyDescriptorPool(pRenderer->device, pRenderer->descriptorPool, nullptr);
	vkDestroyCommandPool(pRenderer->device, pRenderer->commandPool, nullptr);
	pRenderer->commandPool = VK_NULL_HANDLE;
	vkDestroyDevice(pRenderer->device, nullptr);
	pRenderer->device = VK_NULL_HANDLE;
	DestroyInstance(a_ppRenderer);
	ExitWindow(&pRenderer->window);
}

#pragma region VULKAN_IMPLEMENTATION

#pragma region INSTANCE

#if defined(_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData
)
{
	LOG_IF((messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT), LogSeverity::WARNING, "%s", pCallbackData->pMessage);
	return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
{
	debugCreateInfo = {};
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugCallback;
	debugCreateInfo.pUserData = nullptr; // Optional
}
#endif

void CreateInstance(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	const char* instanceExtensions[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#if defined(__ANDROID_API__)
		VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#endif
	};
	
	uint32_t instanceExtensionsCount = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]);
	std::vector<const char*> requiredExtensions(instanceExtensions, instanceExtensions + instanceExtensionsCount);

#if defined(_DEBUG)
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	bool enableValidation = true;
	std::vector<const char*> validationLayersToCheck = {
		"VK_LAYER_KHRONOS_validation"
	};

	enableValidation = validationLayersToCheck.size() > 0;
	for(uint32_t i=0; (i < (uint32_t)validationLayersToCheck.size()) && enableValidation; ++i)
	{
		bool layerFound = false;

		for (const VkLayerProperties& layerProperties : availableLayers)
		{
			if (strcmp(validationLayersToCheck[i], layerProperties.layerName) == 0)
			{
				layerFound = true;
				pRenderer->validationLayers.emplace_back(validationLayersToCheck[i]);
				break;
			}
		}

		if (!layerFound)
		{
			enableValidation = false;
			pRenderer->validationLayers.clear();
		}
	}

	if (enableValidation)
	{
		requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	else
	{
		LOG(LogSeverity::WARNING, "Validation layer is not found!");
	}
#endif

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "AGame";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(pRenderer->validationLayers.size());
	createInfo.ppEnabledLayerNames = pRenderer->validationLayers.data();

	LOG_IF(vkCreateInstance(&createInfo, nullptr, &pRenderer->instance) == VK_SUCCESS, LogSeverity::ERR, "Unable to create Vulkan instance");

	// Debug Messenger
#ifdef _DEBUG
	if (enableValidation)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		populateDebugMessengerCreateInfo(debugCreateInfo);
		PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(pRenderer->instance, "vkCreateDebugUtilsMessengerEXT");

		LOG_IF(func, LogSeverity::WARNING, "Function not available: vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			LOG_IF(func(pRenderer->instance, &debugCreateInfo, nullptr, &pRenderer->debugMessenger) == VK_SUCCESS, LogSeverity::ERR, "Failed to set up debug messenger!");
		}
	}
#endif

	// SURFACE
#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR add_info = {};
	add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.hinstance = ::GetModuleHandle(NULL);
	add_info.hwnd = (HWND)(pRenderer->window.pWindowHandle);
	LOG_IF(vkCreateWin32SurfaceKHR(pRenderer->instance, &add_info, NULL, &pRenderer->surface) == VK_SUCCESS, LogSeverity::ERR, "Failed to create surface!");
#elif defined(__ANDROID_API__)
	VkAndroidSurfaceCreateInfoKHR add_info = {};
	add_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	add_info.pNext = NULL;
	add_info.flags = 0;
	add_info.window = (ANativeWindow*)pRenderer->window.pWindowHandle;
	LOG_IF(vkCreateAndroidSurfaceKHR(pRenderer->instance, &add_info, NULL, &pRenderer->surface) == VK_SUCCESS, LogSeverity::ERR, "Failed to create surface!");
#endif
}

void DestroyInstance(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	vkDestroySurfaceKHR(pRenderer->instance, pRenderer->surface, nullptr);

#if defined(_DEBUG)
	if (pRenderer->debugMessenger)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(pRenderer->instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(pRenderer->instance, pRenderer->debugMessenger, nullptr);
		}
	}
#endif

	vkDestroyInstance(pRenderer->instance, nullptr);
	pRenderer->instance = VK_NULL_HANDLE;
}

#pragma endregion

#pragma region DEVICES

static int max_device_score = 0;

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
{
	SwapChainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

struct QueueFamilyIndices
{
	uint32_t graphicsFamily;
	uint32_t presentFamily;
};
static struct QueueFamilyIndices familyIndices = { (uint32_t)-1, (uint32_t)-1 };

const char* deviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

int rateDeviceSuitability(const VkPhysicalDevice& device, const VkSurfaceKHR& surface)
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	// Application can't function without geometry shaders
	if (!deviceFeatures.geometryShader || !deviceFeatures.samplerAnisotropy)
	{
		return 0;
	}


	int score = 0;

	// Discrete GPUs have a significant performance advantage
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}

	// Maximum possible size of textures affects graphics quality
	score += deviceProperties.limits.maxImageDimension2D;


	// QUEUE FAMILIES CHECK
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
	{
		// GRAPHICS FAMILY
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			familyIndices.graphicsFamily = i;
		}

		// PRESENTATION FAMILY
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (queueFamily.queueCount > 0 && presentSupport)
		{
			familyIndices.presentFamily = i;
		}
		break;

		++i;
	}
	// END QUEUE FAMILIES CHECK


	// device extension support
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::unordered_set<std::string> requiredExtensions;
	int size = sizeof(deviceExtensions) / sizeof(const char*);
	for (int i = 0; i < size; ++i)
	{
		requiredExtensions.insert(deviceExtensions[i]);
	}

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	// END device extension support


	// swapchain support
	bool swapChainAdequate = false;
	if (requiredExtensions.empty())
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}
	// END swapchain support


	if (-1 == familyIndices.graphicsFamily || -1 == familyIndices.presentFamily || !requiredExtensions.empty() || !swapChainAdequate)
		return 0;

	LOG(LogSeverity::INFO, "Device: %s\tScore: %d", deviceProperties.deviceName, score);

	if (max_device_score < score)
		max_device_score = score;

	return score;
}

void PickPhysicalDevice(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;
	
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(pRenderer->instance, &deviceCount, nullptr);
	LOG_IF(deviceCount != 0, LogSeverity::ERR, "Failed to find GPUs with Vulkan support!");
	
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(pRenderer->instance, &deviceCount, devices.data());

	std::unordered_map<int, VkPhysicalDevice> candidates;

	for (const VkPhysicalDevice& device : devices)
	{
		int score = rateDeviceSuitability(device, pRenderer->surface);
		candidates.insert(std::pair<int, VkPhysicalDevice>(score, device));
	}

	// Check if the best candidate is suitable at all
	LOG_IF((max_device_score > 0), LogSeverity::ERR, "failed to find a suitable GPU!");
	pRenderer->physicalDevice = candidates[max_device_score];
	LOG_IF((pRenderer->physicalDevice != VK_NULL_HANDLE), LogSeverity::ERR, "failed to find a suitable GPU!");

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(pRenderer->physicalDevice, &deviceProperties);
}

void CreateLogicalDevice(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;
	
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::unordered_set<uint32_t> uniqueQueueFamilies;
	uniqueQueueFamilies.insert(familyIndices.graphicsFamily);
	uniqueQueueFamilies.insert(familyIndices.presentFamily);

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// queues
	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	// features
	VkPhysicalDeviceFeatures deviceFeatures = {};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.shaderSampledImageArrayDynamicIndexing = VK_TRUE;

	createInfo.pEnabledFeatures = &deviceFeatures;

	// additional extension features
	/*VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexFeature = {};
	indexFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	indexFeature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	indexFeature.runtimeDescriptorArray = VK_TRUE;
	indexFeature.pNext = nullptr;*/

	//createInfo.pNext = &indexFeature;

	// extensions
	createInfo.enabledExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(const char*));
	createInfo.ppEnabledExtensionNames = deviceExtensions;
	// validation layers
	createInfo.enabledLayerCount = static_cast<uint32_t>(pRenderer->validationLayers.size());
	createInfo.ppEnabledLayerNames = pRenderer->validationLayers.data();

	LOG_IF( (vkCreateDevice(pRenderer->physicalDevice, &createInfo, nullptr, &pRenderer->device) == VK_SUCCESS), LogSeverity::ERR, "failed to create logical device!" );
	vkGetDeviceQueue(pRenderer->device, familyIndices.graphicsFamily, 0, &pRenderer->graphicsQueue);
	vkGetDeviceQueue(pRenderer->device, familyIndices.presentFamily, 0, &pRenderer->presentQueue);
}

#pragma endregion

#pragma region SWAPCHAIN

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const VkSurfaceFormatKHR& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			bestMode = availablePresentMode;
		}
	}

	return bestMode;
}

VkExtent2D chooseSwapExtent(Renderer* a_pRenderer, const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent = {
			static_cast<uint32_t>(a_pRenderer->window.width),
			static_cast<uint32_t>(a_pRenderer->window.height)
		};

		actualExtent.width = MAX(capabilities.minImageExtent.width, MIN(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = MAX(capabilities.minImageExtent.height, MIN(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

void CreateSwapchain(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pRenderer->physicalDevice, pRenderer->surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(pRenderer, swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = pRenderer->surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	uint32_t queueFamilyIndices[] = { familyIndices.graphicsFamily, familyIndices.presentFamily };
	if (familyIndices.graphicsFamily != familyIndices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = static_cast<uint32_t>(sizeof(queueFamilyIndices) / sizeof(uint32_t));
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	if (swapChainSupport.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}

	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;
	LOG_IF((vkCreateSwapchainKHR(pRenderer->device, &createInfo, nullptr, &pRenderer->swapChain) == VK_SUCCESS), LogSeverity::ERR, "failed to create swap chain!");

	// Images and Imageviews
	vkGetSwapchainImagesKHR(pRenderer->device, pRenderer->swapChain, &imageCount, nullptr);
	pRenderer->swapchainRenderTargetCount = imageCount;

	pRenderer->swapchainRenderTargets = (RenderTarget**)malloc(sizeof(RenderTarget*) * imageCount);
	void* pool = (RenderTarget*)malloc(sizeof(RenderTarget) * imageCount);
	for (uint32_t i = 0; i < imageCount; ++i)
	{
		pRenderer->swapchainRenderTargets[i] = (RenderTarget*)(pool)+i;
	}

	std::vector<VkImage> swapchainImages(imageCount);
	vkGetSwapchainImagesKHR(pRenderer->device, pRenderer->swapChain, &imageCount, swapchainImages.data());
	
	CommandBuffer cmdBfr;
	BeginSingleTimeCommands(pRenderer, &cmdBfr);

	for (size_t i = 0; i < imageCount; i++)
	{
		pRenderer->swapchainRenderTargets[i]->pTexture = new Texture();
		LOG_IF(pRenderer->swapchainRenderTargets[i]->pTexture, LogSeverity::ERR, "pTexture is uninitialized!");

		pRenderer->swapchainRenderTargets[i]->id = RT_IDs++;
		pRenderer->swapchainRenderTargets[i]->pTexture->desc.format = surfaceFormat.format;
		pRenderer->swapchainRenderTargets[i]->pTexture->desc.width = extent.width;
		pRenderer->swapchainRenderTargets[i]->pTexture->desc.height = extent.height;
		pRenderer->swapchainRenderTargets[i]->pTexture->desc.aspectBits = VK_IMAGE_ASPECT_COLOR_BIT;
		pRenderer->swapchainRenderTargets[i]->pTexture->image = swapchainImages[i];
		pRenderer->swapchainRenderTargets[i]->pTexture->imageView = CreateImageView(pRenderer, swapchainImages[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);

		TransitionImageLayout(&cmdBfr, pRenderer->swapchainRenderTargets[i]->pTexture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	EndSingleTimeCommands(pRenderer, &cmdBfr);
}

void DestroySwapchain(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	for (uint32_t i=0; i < pRenderer->swapchainRenderTargetCount; ++i)
	{
		vkDestroyImageView(pRenderer->device, pRenderer->swapchainRenderTargets[i]->pTexture->imageView, nullptr);
		free(pRenderer->swapchainRenderTargets[i]->pTexture);
	}
	free(pRenderer->swapchainRenderTargets[0]);
	free(pRenderer->swapchainRenderTargets);

	vkDestroySwapchainKHR(pRenderer->device, pRenderer->swapChain, nullptr);
	pRenderer->swapChain = VK_NULL_HANDLE;
}

#pragma endregion

void WaitDeviceIdle(Renderer* a_pRenderer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	vkDeviceWaitIdle(a_pRenderer->device);
}

void CreateSyncObjects(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;
	
	pRenderer->imageAvailableSemaphores.resize(pRenderer->maxInFlightFrames);
	pRenderer->renderFinishedSemaphores.resize(pRenderer->maxInFlightFrames);
	pRenderer->inFlightFences.resize(pRenderer->maxInFlightFrames);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < pRenderer->maxInFlightFrames; i++)
	{
		LOG_IF(
			(	vkCreateSemaphore(pRenderer->device, &semaphoreInfo, nullptr, &pRenderer->imageAvailableSemaphores[i]) == VK_SUCCESS &&
				vkCreateSemaphore(pRenderer->device, &semaphoreInfo, nullptr, &pRenderer->renderFinishedSemaphores[i]) == VK_SUCCESS &&
				vkCreateFence(pRenderer->device, &fenceInfo, nullptr, &pRenderer->inFlightFences[i]) == VK_SUCCESS
			), LogSeverity::ERR, "failed to create semaphores/fences!"
		);
	}
}

void BeginSingleTimeCommands(Renderer* a_pRenderer, CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(a_pRenderer->commandPool, LogSeverity::ERR, "commandPool is NULL");

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = a_pRenderer->commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(a_pRenderer->device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	a_pCommandBuffer->commandBuffer = commandBuffer;
	a_pCommandBuffer->pRenderer = a_pRenderer;
}

void EndSingleTimeCommands(Renderer* a_pRenderer, CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(a_pRenderer->commandPool, LogSeverity::ERR, "commandPool is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer, LogSeverity::ERR, "commandBuffer is NULL");

	vkEndCommandBuffer(a_pCommandBuffer->commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &(a_pCommandBuffer->commandBuffer);

	vkQueueSubmit(a_pRenderer->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(a_pRenderer->graphicsQueue);

	vkFreeCommandBuffers(a_pRenderer->device, a_pRenderer->commandPool, 1, &(a_pCommandBuffer->commandBuffer));
}

#pragma region RESOURCES

#pragma region TEXTURE

uint32_t findMemoryType(Renderer* a_pRenderer, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(a_pRenderer->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	LOG(LogSeverity::ERR, "Failed to find suitable memory type!");
	return -1;
}

VkImageView CreateImageView(Renderer* a_pRenderer, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView = {};
	LOG_IF((vkCreateImageView(a_pRenderer->device, &viewInfo, nullptr, &imageView) == VK_SUCCESS), LogSeverity::ERR, "failed to create texture image view!");

	return imageView;
}

void CreateTexture(Renderer* a_pRenderer, Texture** a_ppTexture)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "Value at a_pRenderer is NULL");
	LOG_IF(a_ppTexture, LogSeverity::ERR, "Value at a_ppTexture is NULL");

	Texture* pTexture = *a_ppTexture;
	VkImage image;
	VkDeviceMemory imageMemory;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = pTexture->desc.width;
	imageInfo.extent.height = pTexture->desc.height;
	imageInfo.extent.depth = 1;
	imageInfo.samples = pTexture->desc.sampleCount;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = pTexture->desc.format;
	imageInfo.tiling = pTexture->desc.tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = pTexture->desc.usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // Optional

	LOG_IF((vkCreateImage(a_pRenderer->device, &imageInfo, nullptr, &image) == VK_SUCCESS), LogSeverity::ERR, "failed to create image!");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(a_pRenderer->device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(a_pRenderer, memRequirements.memoryTypeBits, pTexture->desc.properties);

	LOG_IF((vkAllocateMemory(a_pRenderer->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS), LogSeverity::ERR, "failed to allocate image memory!");

	vkBindImageMemory(a_pRenderer->device, image, imageMemory, 0);

	pTexture->image = image;
	pTexture->imageMemory = imageMemory;
	pTexture->imageView = CreateImageView(a_pRenderer, image, pTexture->desc.format, pTexture->desc.aspectBits);
}

void DestroyTexture(Renderer* a_pRenderer, Texture** a_ppTexture)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppTexture, LogSeverity::ERR, "Value at a_ppTexture is NULL");
	vkDestroyImageView(a_pRenderer->device, (*a_ppTexture)->imageView, nullptr);
	vkDestroyImage(a_pRenderer->device, (*a_ppTexture)->image, nullptr);
	(*a_ppTexture)->imageView = VK_NULL_HANDLE;
	(*a_ppTexture)->image = VK_NULL_HANDLE;
}

bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void TransitionImageLayout(CommandBuffer* a_pCommandBuffer, Texture* a_pTexture, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = a_pTexture->image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(a_pTexture->desc.format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	switch (oldLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		barrier.srcAccessMask = 0;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		break;

	default:
		// layout not handled yet
		break;
	}

	switch (newLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		break;

	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;

	default:
		// layout not handled yet
		break;
	}

	vkCmdPipelineBarrier(
		a_pCommandBuffer->commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

#pragma endregion

#pragma region BUFFER

void CopyBuffer(Renderer* a_pRenderer, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	CommandBuffer cmdBfr;
	BeginSingleTimeCommands(a_pRenderer, &cmdBfr);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBfr.commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	EndSingleTimeCommands(a_pRenderer, &cmdBfr);
}

void CreateBufferUtil(Renderer* a_pRenderer, Buffer** a_ppBuffer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppBuffer, LogSeverity::ERR, "Value at a_ppBuffer is NULL");

	Buffer* pBuffer = *a_ppBuffer;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = pBuffer->desc.bufferSize;
	bufferInfo.usage = pBuffer->desc.bufferUsageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.flags = 0;
	bufferInfo.pNext = NULL;
	bufferInfo.pQueueFamilyIndices = NULL;
	bufferInfo.queueFamilyIndexCount = 0;
	LOG_IF(vkCreateBuffer(a_pRenderer->device, &bufferInfo, nullptr, &pBuffer->buffer) == VK_SUCCESS, LogSeverity::ERR, "Failed to create a buffer");

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(a_pRenderer->device, pBuffer->buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(a_pRenderer, memRequirements.memoryTypeBits, pBuffer->desc.memoryPropertyFlags);
	LOG_IF(vkAllocateMemory(a_pRenderer->device, &allocInfo, nullptr, &pBuffer->bufferMemory) == VK_SUCCESS, LogSeverity::ERR, "failed to allocate buffer memory!");

	LOG_IF(vkBindBufferMemory(a_pRenderer->device, pBuffer->buffer, pBuffer->bufferMemory, 0) == VK_SUCCESS, LogSeverity::ERR, "failed to bind buffer memory");
}

void CreateBuffer(Renderer* a_pRenderer, Buffer** a_ppBuffer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppBuffer, LogSeverity::ERR, "Value at a_ppBuffer is NULL");

	Buffer* pBuffer = *a_ppBuffer;

	if ((pBuffer->desc.memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && pBuffer->desc.pData)
	{
		VkBufferUsageFlags usageFlags = pBuffer->desc.bufferUsageFlags;
		pBuffer->desc.bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		CreateBufferUtil(a_pRenderer, a_ppBuffer);
		pBuffer->desc.bufferUsageFlags = usageFlags;

		{
			// create staging buffer
			Buffer* pStagingBuffer = new Buffer();
			pStagingBuffer->desc.bufferSize = pBuffer->desc.bufferSize;
			pStagingBuffer->desc.bufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			pStagingBuffer->desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			CreateBufferUtil(a_pRenderer, &pStagingBuffer);

			// push data to staging buffer
			void* data;
			vkMapMemory(a_pRenderer->device, pStagingBuffer->bufferMemory, 0, pStagingBuffer->desc.bufferSize, 0, &data);
			memcpy(data, pStagingBuffer->desc.pData, (size_t)pStagingBuffer->desc.bufferSize);
			vkUnmapMemory(a_pRenderer->device, pStagingBuffer->bufferMemory);

			// copy from staging to device local buffer
			CopyBuffer(a_pRenderer, pStagingBuffer->buffer, pBuffer->buffer, pStagingBuffer->desc.bufferSize);

			DestroyBuffer(a_pRenderer, &pStagingBuffer);
			delete pStagingBuffer;
		}
	}
	if (pBuffer->desc.memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		VkMemoryPropertyFlags memoryPropertyFlags = pBuffer->desc.memoryPropertyFlags;
		memoryPropertyFlags = pBuffer->desc.memoryPropertyFlags;
		pBuffer->desc.memoryPropertyFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		CreateBufferUtil(a_pRenderer, a_ppBuffer);
		
		pBuffer->desc.memoryPropertyFlags = memoryPropertyFlags;

		if (pBuffer->desc.pData)
		{
			void* data;
			vkMapMemory(a_pRenderer->device, pBuffer->bufferMemory, 0, pBuffer->desc.bufferSize, 0, &data);
			memcpy(data, pBuffer->desc.pData, (size_t)pBuffer->desc.bufferSize);
			vkUnmapMemory(a_pRenderer->device, pBuffer->bufferMemory);
		}
	}
}

void DestroyBuffer(Renderer* a_pRenderer, Buffer** a_ppBuffer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppBuffer, LogSeverity::ERR, "Value at a_ppBuffer is NULL");
	Buffer* pBuffer = (*a_ppBuffer);

	vkDestroyBuffer(a_pRenderer->device, pBuffer->buffer, nullptr);
	vkFreeMemory(a_pRenderer->device, pBuffer->bufferMemory, nullptr);
	pBuffer->buffer = VK_NULL_HANDLE;
	pBuffer->bufferMemory = VK_NULL_HANDLE;
}

void UpdateBuffer(Renderer* a_pRenderer, Buffer* a_pBuffer, void* a_pData, uint64_t a_uSize)
{
	if (!a_pData || (a_uSize <= 0))
		return;

	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(a_pBuffer, LogSeverity::ERR, "a_pBuffer is NULL");
	
	void* data;
	vkMapMemory(a_pRenderer->device, a_pBuffer->bufferMemory, 0, a_uSize, 0, &data);
	memcpy(data, a_pData, (size_t)a_uSize);
	vkUnmapMemory(a_pRenderer->device, a_pBuffer->bufferMemory);
}

#pragma endregion

#pragma endregion

void CreateRenderPass(Renderer* a_pRenderer, LoadActionsDesc* pLoadActions, RenderPass** a_ppRenderPass)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppRenderPass, LogSeverity::ERR, "Value at a_ppRenderPass is NULL");

	RenderPass* pRenderPass = *a_ppRenderPass;
	uint32_t colorAttachmentsCount = (uint32_t)pRenderPass->desc.colorFormats.size();
	uint32_t depthAttachmentCount = (pRenderPass->desc.depthStencilFormat == VK_FORMAT_UNDEFINED) ? 0 : 1;

	std::vector<VkAttachmentReference> attachmentRefs(colorAttachmentsCount + depthAttachmentCount);
	std::vector<VkAttachmentDescription> attachments(colorAttachmentsCount + depthAttachmentCount);
	
	// COLOR ATTACHMENTS
	for (uint32_t i = 0; i < colorAttachmentsCount; ++i)
	{
		attachments[i].format = pRenderPass->desc.colorFormats[i];
		attachments[i].samples = pRenderPass->desc.sampleCount;
		attachments[i].loadOp = pLoadActions ? pLoadActions->loadColorActions[i] : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[i].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[i].flags = 0;

		attachmentRefs[i].attachment = i;
		attachmentRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	// DEPTH/STENCIL ATTACHMENT
	if (depthAttachmentCount > 0)
	{
		attachments[colorAttachmentsCount].format = pRenderPass->desc.depthStencilFormat;
		attachments[colorAttachmentsCount].samples = pRenderPass->desc.sampleCount;
		attachments[colorAttachmentsCount].loadOp = pLoadActions->loadDepthAction;
		attachments[colorAttachmentsCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[colorAttachmentsCount].stencilLoadOp = pLoadActions->loadStencilAction;
		attachments[colorAttachmentsCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[colorAttachmentsCount].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[colorAttachmentsCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[colorAttachmentsCount].flags = 0;

		attachmentRefs[colorAttachmentsCount].attachment = colorAttachmentsCount;
		attachmentRefs[colorAttachmentsCount].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorAttachmentsCount;
	subpass.pColorAttachments = attachmentRefs.data();
	subpass.pDepthStencilAttachment = (depthAttachmentCount > 0) ? &(attachmentRefs[colorAttachmentsCount]) : nullptr;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.pResolveAttachments = NULL;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = NULL;

	LOG_IF( (vkCreateRenderPass(a_pRenderer->device, &renderPassInfo, nullptr, &pRenderPass->renderPass) == VK_SUCCESS), LogSeverity::ERR, "failed to create render pass!" );
}

void DestroyRenderPass(Renderer* a_pRenderer, RenderPass** a_ppRenderPass)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppRenderPass, LogSeverity::ERR, "Value at a_ppRenderPass is NULL");

	vkDestroyRenderPass(a_pRenderer->device, (*a_ppRenderPass)->renderPass, nullptr);
	*a_ppRenderPass = VK_NULL_HANDLE;
}

#pragma region DESCRIPTORS

void CreateDescriptorPool(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, " Value at a_ppRenderer is NULL");

	Renderer* pRenderer = *a_ppRenderer;
	VkDescriptorPoolSize descriptorPoolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = sizeof(descriptorPoolSizes) / sizeof(descriptorPoolSizes[0]);
	poolInfo.pPoolSizes = descriptorPoolSizes;
	poolInfo.maxSets = 8192;
	LOG_IF( (vkCreateDescriptorPool(pRenderer->device, &poolInfo, nullptr, &pRenderer->descriptorPool) == VK_SUCCESS),
		LogSeverity::ERR, "Failed to create descriptor pool" );
}

void DestroyDescriptorPool(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, " Value at a_ppRenderer is NULL");
	vkDestroyDescriptorPool((*a_ppRenderer)->device, (*a_ppRenderer)->descriptorPool, nullptr);
}

void CreateResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppResourceDescriptor, LogSeverity::ERR, "Value at a_ppResourceDescriptor is NULL");

	ResourceDescriptor* pResourceDescriptor = *a_ppResourceDescriptor;

	// sort layouts by set and binding indexes
	std::vector<DescriptorInfo> descriptors = pResourceDescriptor->desc.descriptors;
	std::sort(descriptors.begin(), descriptors.end(), [](DescriptorInfo a, DescriptorInfo b) { return a.binding.binding < b.binding.binding; });
	std::sort(descriptors.begin(), descriptors.end(), [](DescriptorInfo a, DescriptorInfo b) { return a.set < b.set; });

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings[(uint32_t)DescriptorUpdateFrequency::COUNT] = {};
	uint32_t descriptorCounts[(uint32_t)DescriptorUpdateFrequency::COUNT] = { 0 };

	for (DescriptorInfo& desc : descriptors)
	{
		layoutBindings[desc.set].push_back(desc.binding);					// list of descriptor bindings
		pResourceDescriptor->descriptorInfos[desc.set].push_back(desc);		// store descriptor info in a list of it's set index
		pResourceDescriptor->nameToDescriptorInfoIndexMap.insert({ (uint32_t)std::hash<std::string>{}(desc.name) , descriptorCounts[desc.set]++ });
	}

	uint32_t layoutsCount = 0;
	for (int i = (int)DescriptorUpdateFrequency::COUNT-1; i >= 0; --i)
	{
		const std::vector<VkDescriptorSetLayoutBinding>& binding = layoutBindings[i];

		bool createLayout = binding.size() > 0;
		if (!createLayout && i < (int)DescriptorUpdateFrequency::COUNT - 1)
		{
			createLayout = pResourceDescriptor->descriptorSetLayouts[i + 1] != VK_NULL_HANDLE;
		}

		if (createLayout)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo = {};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(binding.size());
			layoutInfo.pBindings = binding.data();
			LOG_IF((vkCreateDescriptorSetLayout(a_pRenderer->device, &layoutInfo, nullptr, &pResourceDescriptor->descriptorSetLayouts[i]) == VK_SUCCESS),
				LogSeverity::ERR, "failed to create descriptor set layout!");
			++layoutsCount;
		}
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = layoutsCount;
	pipelineLayoutInfo.pSetLayouts = pResourceDescriptor->descriptorSetLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	LOG_IF((vkCreatePipelineLayout(a_pRenderer->device, &pipelineLayoutInfo, nullptr, &pResourceDescriptor->pipelineLayout) == VK_SUCCESS),
		LogSeverity::ERR, "failed to create pipeline layout!");

	for (uint32_t i = 0; i < (uint32_t)DescriptorUpdateFrequency::COUNT; ++i)
	{
		const std::vector<VkDescriptorSetLayoutBinding>& binding = layoutBindings[i];
		if (binding.empty())
			continue;

		std::vector<VkDescriptorUpdateTemplateEntry> descriptorUpdateTemplateEntries(binding.size());

		uint32_t offset = 0;
		for (uint32_t j = 0; j < binding.size(); ++j)
		{
			descriptorUpdateTemplateEntries[i].descriptorCount = binding[j].descriptorCount;
			descriptorUpdateTemplateEntries[i].descriptorType = binding[j].descriptorType;
			descriptorUpdateTemplateEntries[i].dstArrayElement = 0;
			descriptorUpdateTemplateEntries[i].dstBinding = binding[j].binding;
			descriptorUpdateTemplateEntries[i].offset = offset;
			descriptorUpdateTemplateEntries[i].stride = sizeof(DescriptorUpdateData);
			offset += sizeof(DescriptorUpdateData);
		}

		VkDescriptorUpdateTemplateCreateInfo createInfo = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO,  // sType
			NULL,                                                      // pNext
			0,                                                         // flags
			(uint32_t)descriptorUpdateTemplateEntries.size(),          // descriptorUpdateEntryCount
			descriptorUpdateTemplateEntries.data(),                    // pDescriptorUpdateEntries
			VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET,         // templateType
			pResourceDescriptor->descriptorSetLayouts[i],              // descriptorSetLayout
			VK_PIPELINE_BIND_POINT_GRAPHICS,                           // pipelineBindPoint, ignored by given templateType
			pResourceDescriptor->pipelineLayout,                       // pipelineLayout, ignored by given templateType
			i,                                                         // set, ignored by given templateType
		};

		LOG_IF((vkCreateDescriptorUpdateTemplate(a_pRenderer->device, &createInfo, NULL, &pResourceDescriptor->descriptorUpdateTemplates[i]) == VK_SUCCESS),
			LogSeverity::ERR, "failed to create descriptor update template!");
	}
}

void DestroyResourceDescriptor(Renderer* a_pRenderer, ResourceDescriptor** a_ppResourceDescriptor)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppResourceDescriptor, LogSeverity::ERR, "Value at a_ppResourceDescriptor is NULL");
	ResourceDescriptor* pResourceDescriptor = *a_ppResourceDescriptor;

	vkDestroyPipelineLayout(a_pRenderer->device, pResourceDescriptor->pipelineLayout, nullptr);
	pResourceDescriptor->pipelineLayout = VK_NULL_HANDLE;

	for (uint32_t i = 0; i < (uint32_t)DescriptorUpdateFrequency::COUNT; ++i)
	{
		if(pResourceDescriptor->descriptorUpdateTemplates[i] != VK_NULL_HANDLE)
			vkDestroyDescriptorUpdateTemplate(a_pRenderer->device, pResourceDescriptor->descriptorUpdateTemplates[i], nullptr);
		if(pResourceDescriptor->descriptorSetLayouts[i] != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(a_pRenderer->device, pResourceDescriptor->descriptorSetLayouts[i], nullptr);

		pResourceDescriptor->descriptorUpdateTemplates[i] = VK_NULL_HANDLE;
		pResourceDescriptor->descriptorSetLayouts[i] = VK_NULL_HANDLE;
		pResourceDescriptor->descriptorInfos[i].clear();
	}
}

void CreateDescriptorSet(Renderer* a_pRenderer, DescriptorSet** a_ppDescriptorSet)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppDescriptorSet, LogSeverity::ERR, "Value at a_ppDescriptorSet is NULL");

	DescriptorSet* pDescriptorSet = *a_ppDescriptorSet;
	ResourceDescriptor* pResDesc = pDescriptorSet->desc.pResourceDescriptor;
	uint32_t updateFrequency = (uint32_t)pDescriptorSet->desc.updateFrequency;
	LOG_IF(pResDesc->descriptorSetLayouts[updateFrequency],
		LogSeverity::ERR, "For %u update frequency descriptorSetLayouts is NULL", (uint32_t)updateFrequency);
	std::vector<VkDescriptorSetLayout> layouts(pDescriptorSet->desc.maxSet, pResDesc->descriptorSetLayouts[updateFrequency]);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = a_pRenderer->descriptorPool;
	allocInfo.descriptorSetCount = pDescriptorSet->desc.maxSet;
	allocInfo.pSetLayouts = layouts.data();
	allocInfo.pNext = nullptr;

	pDescriptorSet->descriptorSets.resize(pDescriptorSet->desc.maxSet);
	pDescriptorSet->updateData.resize(pDescriptorSet->desc.maxSet * pResDesc->descriptorInfos[updateFrequency].size());

	LOG_IF( (vkAllocateDescriptorSets(a_pRenderer->device, &allocInfo, pDescriptorSet->descriptorSets.data()) == VK_SUCCESS),
		LogSeverity::ERR, "failed to allocate descriptor set");
}

void DestroyDescriptorSet(Renderer* a_pRenderer, DescriptorSet** a_ppDescriptorSet)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppDescriptorSet, LogSeverity::ERR, "Value at a_ppDescriptorSet is NULL");
	DescriptorSet* pDescriptorSet = *a_ppDescriptorSet;
	pDescriptorSet->descriptorSets.clear();
	pDescriptorSet->updateData.clear();
}

void UpdateDescriptorSet(Renderer* a_pRenderer, uint32_t index, DescriptorSet* a_pDescriptorSet, uint32_t a_uCount, const DescriptorUpdateInfo* a_pDescriptorUpdateInfos)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(a_pDescriptorSet, LogSeverity::ERR, "a_pDescriptorSet is NULL");
	LOG_IF(a_pDescriptorUpdateInfos, LogSeverity::ERR, "a_pDescriptorUpdateInfos is NULL");

	ResourceDescriptor* pResourceDescriptor = a_pDescriptorSet->desc.pResourceDescriptor;
	uint32_t updateFrequency = (uint32_t)a_pDescriptorSet->desc.updateFrequency;
	DescriptorUpdateData* pUpdateData = nullptr;

	for (uint32_t i = 0; i < a_uCount; ++i)
	{
		const DescriptorUpdateInfo& pInfo = a_pDescriptorUpdateInfos[i];
		uint32_t nameHash = (uint32_t)std::hash<std::string>{}(pInfo.name);
		uint32_t descIndex = (uint32_t)-1;

		std::unordered_map<uint32_t, uint32_t>::const_iterator itr = pResourceDescriptor->nameToDescriptorInfoIndexMap.find(nameHash);
		if (itr != pResourceDescriptor->nameToDescriptorInfoIndexMap.end())
		{
			descIndex = itr->second;
		}
		else
		{
			LOG(LogSeverity::ERR, "Descriptor of name %s not found!", pInfo.name.c_str());
			return;
		}

		DescriptorInfo& descInfo = pResourceDescriptor->descriptorInfos[updateFrequency][descIndex];
		uint32_t updateIndexOffset = ((uint32_t)pResourceDescriptor->descriptorInfos[updateFrequency].size() * index) + descIndex;
		pUpdateData = &(a_pDescriptorSet->updateData[updateIndexOffset]);

		switch (descInfo.binding.descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			pUpdateData->mBufferInfo = a_pDescriptorUpdateInfos[i].mBufferInfo;
			break;
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			pUpdateData->mImageInfo = a_pDescriptorUpdateInfos[i].mImageInfo;
			break;
		default:
			break;
		}
	}

	vkUpdateDescriptorSetWithTemplate(a_pRenderer->device, a_pDescriptorSet->descriptorSets[index],
		a_pDescriptorSet->desc.pResourceDescriptor->descriptorUpdateTemplates[updateFrequency], pUpdateData);
}

void BindDescriptorSet(CommandBuffer* a_pCommandBuffer, uint32_t a_uIndex, DescriptorSet* a_pDescriptorSet)
{
	LOG_IF(a_pDescriptorSet, LogSeverity::ERR, "a_pDescriptorSet is NULL");

	ResourceDescriptor* pResourceDescriptor = a_pDescriptorSet->desc.pResourceDescriptor;
	uint32_t updateFrequency = (uint32_t)a_pDescriptorSet->desc.updateFrequency;
	vkCmdBindDescriptorSets(a_pCommandBuffer->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pResourceDescriptor->pipelineLayout, updateFrequency, 1,
		&a_pDescriptorSet->descriptorSets[a_uIndex], 0, NULL);
}

#pragma endregion

void CreateGraphicsPipeline(Renderer* a_pRenderer, Pipeline** a_ppPipeline)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppPipeline, LogSeverity::ERR, "Value at a_ppPipeline is NULL");
	LOG_IF((*a_ppPipeline)->desc.pResourceDescriptor, LogSeverity::ERR, "Value at a_ppPipeline is NULL");
	LOG_IF((*a_ppPipeline)->desc.pResourceDescriptor->pipelineLayout, LogSeverity::ERR, "pipelineLayout is NULL");

	Pipeline* pPipeline = *a_ppPipeline;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	// Shaders
	uint32_t stageCount = (uint32_t)pPipeline->desc.shaders.size();
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages(stageCount);
	{
		for (uint32_t i=0; i < stageCount; ++i)
		{
			shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStages[i].stage = pPipeline->desc.shaders[i]->stage;
			shaderStages[i].module = pPipeline->desc.shaders[i]->shaderModule;
			shaderStages[i].pName = "main";
			shaderStages[i].flags = 0;
			shaderStages[i].pNext = nullptr;
			shaderStages[i].pSpecializationInfo = nullptr;
		}
		pipelineInfo.stageCount = stageCount;
		pipelineInfo.pStages = shaderStages.data();
	}

	// Input Bindings and Attributes
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	{
		std::sort(pPipeline->desc.attribs.begin(), pPipeline->desc.attribs.end(), [](VertexAttribute a, VertexAttribute b) { return a.binding < b.binding; });

		std::vector<VkVertexInputBindingDescription> vertexInputBindings;
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(pPipeline->desc.attribs.size());

		for (uint32_t i = 0; i < pPipeline->desc.attribs.size(); ++i)
		{
			static uint32_t binding_count = 0;
			static uint32_t current_binding = 0;

			if (current_binding != pPipeline->desc.attribs[i].binding)
			{
				++binding_count;
				current_binding = pPipeline->desc.attribs[i].binding;

				VkVertexInputBindingDescription bindingDesc;
				bindingDesc.binding = current_binding;
				bindingDesc.inputRate = pPipeline->desc.attribs[i].inputRate;
				vertexInputBindings.emplace_back(bindingDesc);
			}

			vertexInputBindings.back().stride += pPipeline->desc.attribs[i].stride;

			vertexInputAttributes[i].binding = pPipeline->desc.attribs[i].binding;
			vertexInputAttributes[i].format = pPipeline->desc.attribs[i].format;
			vertexInputAttributes[i].location = pPipeline->desc.attribs[i].location;
			vertexInputAttributes[i].offset = pPipeline->desc.attribs[i].offset;
		}

		vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputStateCreateInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindings.size();
		vertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputStateCreateInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributes.size();
		vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();
		vertexInputStateCreateInfo.flags = 0;
		vertexInputStateCreateInfo.pNext = nullptr;
	}
	pipelineInfo.pVertexInputState = &vertexInputStateCreateInfo;

	// Topology
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	{
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = pPipeline->desc.topology;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
	}
	pipelineInfo.pInputAssemblyState = &inputAssembly;

	// Viewport and Scissor - Dynamic
	VkPipelineViewportStateCreateInfo viewportState = {};
	{
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = NULL;
		viewportState.scissorCount = 1;
		viewportState.pScissors = NULL;
	}
	pipelineInfo.pViewportState = &viewportState;

	// Raster Info
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	{
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.cullMode = pPipeline->desc.cullMode;
		rasterizationState.depthBiasEnable = (pPipeline->desc.depthBias != 0.0f) ? VK_TRUE : VK_FALSE;
		rasterizationState.depthBiasConstantFactor = pPipeline->desc.depthBias;
		rasterizationState.depthBiasSlopeFactor = pPipeline->desc.depthBiasSlope;
		rasterizationState.depthClampEnable = pPipeline->desc.depthClamp ? VK_TRUE : VK_FALSE;
		rasterizationState.frontFace = pPipeline->desc.frontFace;
		rasterizationState.polygonMode = pPipeline->desc.polygonMode;
		rasterizationState.lineWidth = 1;
		rasterizationState.depthBiasClamp = 0.0f;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.flags = 0;
		rasterizationState.pNext = NULL;
	}
	pipelineInfo.pRasterizationState = &rasterizationState;

	// MultiSample
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	{
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = pPipeline->desc.sampleCount;
		multisampling.minSampleShading = 0.0f;
		multisampling.pSampleMask = nullptr;
		multisampling.alphaToCoverageEnable = VK_FALSE;
		multisampling.alphaToOneEnable = VK_FALSE;
	}
	pipelineInfo.pMultisampleState = &multisampling;
	
	// Color Blend State - OFF for now
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	{
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional
	}
	pipelineInfo.pColorBlendState = &colorBlending;

	// Depth/Stencil test - Stencil OFF for now
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	{
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = pPipeline->desc.depthTest ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = pPipeline->desc.depthWrite ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = pPipeline->desc.depthFunction;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;
		depthStencil.maxDepthBounds = 1.0f;
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional
	}
	pipelineInfo.pDepthStencilState = &depthStencil;

	// Dynamic States
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	VkDynamicState dynamicStates[3];
	{
		dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
		dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
		dynamicStates[2] = VK_DYNAMIC_STATE_LINE_WIDTH;

		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = (uint32_t)(sizeof(dynamicStates) / sizeof(dynamicStates[0]));
		dynamicStateInfo.pDynamicStates = dynamicStates;
		dynamicStateInfo.flags = 0;
		dynamicStateInfo.pNext = nullptr;
	}
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	
	// Pipeline Layout
	pipelineInfo.layout = pPipeline->desc.pResourceDescriptor->pipelineLayout;

	// RenderPass
	RenderPass* pRenderPass = new RenderPass();
	{
		pRenderPass->desc.colorFormats = pPipeline->desc.colorFormats;
		pRenderPass->desc.depthStencilFormat = pPipeline->desc.depthStencilFormat;
		pRenderPass->desc.sampleCount = pPipeline->desc.sampleCount;
		CreateRenderPass(a_pRenderer, nullptr, &pRenderPass);
	}
	pipelineInfo.renderPass = pRenderPass->renderPass;

	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	LOG_IF( (vkCreateGraphicsPipelines(a_pRenderer->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &(pPipeline->pipeline)) == VK_SUCCESS),
		LogSeverity::ERR, "Failed to create graphics pipeline!" );

	DestroyRenderPass(a_pRenderer, &pRenderPass);
	delete pRenderPass;
}

void DestroyGraphicsPipeline(Renderer* a_pRenderer, Pipeline** a_ppPipeline)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppPipeline, LogSeverity::ERR, "Value at a_ppPipeline is NULL");
	vkDestroyPipeline(a_pRenderer->device, (*a_ppPipeline)->pipeline, nullptr);
	(*a_ppPipeline)->pipeline = VK_NULL_HANDLE;
}

#pragma region RenderTarget

void CreateRenderTarget(Renderer* a_pRenderer, RenderTarget** a_ppRenderTarget)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppRenderTarget, LogSeverity::ERR, "Value at a_ppRenderTarget is NULL");
	LOG_IF((*a_ppRenderTarget)->pTexture, LogSeverity::ERR, "pTexture is NULL");
	LOG_IF(((*a_ppRenderTarget)->id != INVALID_RT_ID), LogSeverity::ERR, "RenderTarger is already initialized");

	RenderTarget* pRenderTarget = *a_ppRenderTarget;
	pRenderTarget->id = RT_IDs++;
	CreateTexture(a_pRenderer, &pRenderTarget->pTexture);
}

void DestroyRenderTarget(Renderer* a_pRenderer, RenderTarget** a_ppRenderTarget)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppRenderTarget, LogSeverity::ERR, "Value at a_ppRenderTarget is NULL");

	RenderTarget* pRenderTarget = *a_ppRenderTarget;
	pRenderTarget->id = INVALID_RT_ID;
	DestroyTexture(a_pRenderer, &pRenderTarget->pTexture);
}

static inline uint32_t hash(const uint32_t* mem, uint32_t size, uint32_t prev = 2166136261U)
{
	uint32_t result = prev;
	while (size--)
		result = (result * 16777619) ^ *mem++;
	return (uint32_t)result;
}

void BindRenderTargets(CommandBuffer* a_pCommandBuffer, uint32_t a_uRenderTargetCount, RenderTarget** a_ppRenderTargets, LoadActionsDesc* a_pLoadActions, RenderTarget* a_pDepthTarget)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "Value at a_ppCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");

	Renderer* pRenderer = a_pCommandBuffer->pRenderer;

	// end renderpass
	if (a_pCommandBuffer->activeRenderPass != VK_NULL_HANDLE)
	{
		vkCmdEndRenderPass(a_pCommandBuffer->commandBuffer);
		a_pCommandBuffer->activeRenderPass = VK_NULL_HANDLE;
	}

	if (!a_uRenderTargetCount && !a_pDepthTarget)
		return;

	uint32_t renderpassHash = 0;
	uint32_t framebufferHash = 0;
	VkRenderPass renderpass = {};
	VkFramebuffer framebuffer = {};

	for (uint32_t i = 0; i < a_uRenderTargetCount; ++i)
	{
		uint32_t renderPassHashValues[3] = {
			(uint32_t)a_ppRenderTargets[i]->pTexture->desc.format,
			(uint32_t)a_ppRenderTargets[i]->pTexture->desc.sampleCount,
			(uint32_t)a_pLoadActions->loadColorActions[i]
		};

		uint32_t frameBufferHashValues[3] = {
			a_ppRenderTargets[i]->id,
			a_ppRenderTargets[i]->pTexture->desc.width,
			a_ppRenderTargets[i]->pTexture->desc.height
		};

		renderpassHash = hash(renderPassHashValues, sizeof(renderPassHashValues) / sizeof(renderPassHashValues[0]), renderpassHash);
		framebufferHash = hash(frameBufferHashValues, sizeof(frameBufferHashValues) / sizeof(frameBufferHashValues[0]), framebufferHash);
	}

	if (a_pDepthTarget)
	{
		uint32_t renderPassHashValues[4] = {
			(uint32_t)a_pDepthTarget->pTexture->desc.format,
			(uint32_t)a_pDepthTarget->pTexture->desc.sampleCount,
			(a_pLoadActions) ? (uint32_t)a_pLoadActions->loadDepthAction : 0,
			(a_pLoadActions) ? (uint32_t)a_pLoadActions->loadStencilAction : 0
		};

		uint32_t frameBufferHashValues[3] = {
			a_pDepthTarget->id,
			a_pDepthTarget->pTexture->desc.width,
			a_pDepthTarget->pTexture->desc.height
		};

		renderpassHash = hash(renderPassHashValues, sizeof(renderPassHashValues) / sizeof(renderPassHashValues[0]), renderpassHash);
		framebufferHash = hash(frameBufferHashValues, sizeof(frameBufferHashValues) / sizeof(frameBufferHashValues[0]), framebufferHash);
	}

	std::unordered_map<uint32_t, VkRenderPass>::const_iterator rp_itr = renderPasses.find(renderpassHash);
	if (rp_itr == renderPasses.end())
	{
		RenderPass rdPass;

		for (uint32_t i = 0; i < a_uRenderTargetCount; ++i)
		{
			rdPass.desc.colorFormats.emplace_back(a_ppRenderTargets[i]->pTexture->desc.format);
			rdPass.desc.sampleCount = a_ppRenderTargets[i]->pTexture->desc.sampleCount;
		}

		if (a_pDepthTarget)
		{
			rdPass.desc.depthStencilFormat = a_pDepthTarget->pTexture->desc.format;
			rdPass.desc.sampleCount = a_pDepthTarget->pTexture->desc.sampleCount;
		}

		RenderPass* pRenderPass = &rdPass;
		CreateRenderPass(pRenderer, a_pLoadActions, &pRenderPass);
		renderpass = rdPass.renderPass;

		renderPasses.insert({ renderpassHash, renderpass });
	}
	else
	{
		renderpass = rp_itr->second;
	}

	std::unordered_map<uint32_t, VkFramebuffer>::const_iterator fb_itr = frameBuffers.find(framebufferHash);
	if (fb_itr == frameBuffers.end())
	{
		uint32_t width = 0, height = 0, attachmentCount = (uint32_t)a_uRenderTargetCount;
		attachmentCount += (a_pDepthTarget != nullptr) ? 1 : 0;

		std::vector<VkImageView> attachments(attachmentCount);
		for (uint32_t i = 0; i < a_uRenderTargetCount; ++i)
		{
			width = a_ppRenderTargets[i]->pTexture->desc.width;
			height = a_ppRenderTargets[i]->pTexture->desc.height;
			attachments[i] = a_ppRenderTargets[i]->pTexture->imageView;
		}
		if (a_pDepthTarget)
		{
			width = a_pDepthTarget->pTexture->desc.width;
			height = a_pDepthTarget->pTexture->desc.height;
			attachments.back() = a_pDepthTarget->pTexture->imageView;
		}

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderpass;
		framebufferInfo.attachmentCount = (uint32_t)attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;
		LOG_IF( (vkCreateFramebuffer(pRenderer->device, &framebufferInfo, nullptr, &framebuffer) == VK_SUCCESS), LogSeverity::ERR, "Failed to create framebuffer!");

		frameBuffers.insert({ framebufferHash, framebuffer });
	}
	else
	{
		framebuffer = fb_itr->second;
	}

	a_pCommandBuffer->activeRenderPass = renderpass;

	VkExtent2D extent = { 0, 0};
	if (a_uRenderTargetCount)
	{
		extent = { a_ppRenderTargets[0]->pTexture->desc.width,
				   a_ppRenderTargets[0]->pTexture->desc.height };
	}
	else if (a_pDepthTarget)
	{
		extent = { a_pDepthTarget->pTexture->desc.width,
				   a_pDepthTarget->pTexture->desc.height };
	}

	// begin renderpass
	std::vector<VkClearValue> clearValues;
	
	if (a_pLoadActions != nullptr)
	{
		if (a_pLoadActions->clearColors.size())
			clearValues.insert(clearValues.end(), a_pLoadActions->clearColors.begin(), a_pLoadActions->clearColors.end());
	
		if (a_pDepthTarget)
			clearValues.emplace_back(a_pLoadActions->clearDepth);
	}

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderpass;
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = extent;
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(a_pCommandBuffer->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

#pragma endregion

void CreateShaderModule(Renderer* a_pRenderer, const char* a_sPath, ShaderModule** a_ppShaderModule)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppShaderModule, LogSeverity::ERR, "Value at a_ppShaderModule is NULL");
	LOG_IF(a_sPath, LogSeverity::ERR, "a_sPath is NULL");

	ShaderModule* pShaderModule = *a_ppShaderModule;
	char* buffer = nullptr;

#if defined(_WIN32)
	// convert to spirv
	std::string path = std::string(a_sPath);
	std::replace(path.begin(), path.end(), '/', '\\');

	size_t index = path.find_last_of("\\");
	std::string filePath = path.substr(0, index);
	std::string shaderNameWithExt = path.substr(index + 1);

	std::string vulkan_env = "VULKAN_SDK";
	char* vulkan_env_str_buffer;
	size_t bufferCount;
	_dupenv_s(&vulkan_env_str_buffer, &bufferCount, vulkan_env.c_str());

	char cdbuffer[256];
	GetCurrentDir(cdbuffer, 256);

	std::string glslangValidator = vulkan_env_str_buffer;
	glslangValidator += "\\Bin\\glslangValidator.exe";
	std::string inputFilePath = cdbuffer + std::string("\\") + filePath + std::string("\\") + shaderNameWithExt;
	std::string outputFilePath = cdbuffer + std::string("\\") + filePath + "\\SpirV\\";

	if(!ExistDirectory(outputFilePath.c_str()))
		CreateDirecroty(outputFilePath.c_str());

	std::string cmd = glslangValidator + " -V " + inputFilePath + " -o " + outputFilePath + shaderNameWithExt + ".spv";
	
	STARTUPINFOA        startupInfo;
	PROCESS_INFORMATION processInfo;
	memset(&startupInfo, 0, sizeof startupInfo);
	memset(&processInfo, 0, sizeof processInfo);
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = NULL;
	startupInfo.hStdError = NULL;

	{
		//if (system(cmd.c_str()) != 0)
		//	return;
		if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, NULL, NULL, NULL, NULL, &startupInfo, &processInfo))
			return;
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		DWORD exitCode;
		GetExitCodeProcess(processInfo.hProcess, &exitCode);

		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
	
	
	// create shader module
	FileHandle file = OpenFile((outputFilePath + shaderNameWithExt + ".spv").c_str(), "rb");
	uint32_t fileSize = FileSize(file);
	buffer = (char*)malloc(sizeof(char) * fileSize);
	FileRead(file, &buffer, fileSize);

	std::vector<char> code(buffer, buffer+fileSize);
#endif

#if defined(__ANDROID_API__)
	// Like -DMY_DEFINE=1
	//options.AddMacroDefinition("MY_DEFINE", "1");
	FileHandle file = OpenFile(a_sPath, "r");
	uint32_t fileSize = FileSize(file);
	buffer = (char*)malloc(sizeof(char) * fileSize);
	FileRead(file, &buffer, fileSize);

	shaderc_shader_kind kind = {};
	switch (pShaderModule->stage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:	kind = shaderc_glsl_vertex_shader;		break;
	case VK_SHADER_STAGE_FRAGMENT_BIT:	kind = shaderc_glsl_fragment_shader;	break;
	default: 
		LOG(LogSeverity::ERR, "Shader Type not supported yet!");
		break;
	}

	std::vector<char> code;
	{
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.AddMacroDefinition("TARGET_ANDROID", "1");
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
		shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(buffer, fileSize, kind, "shaderc_error", "main", options);
		LOG_IF((module.GetCompilationStatus() == shaderc_compilation_status_success), LogSeverity::ERR, "SpvCompilation Error: %s", module.GetErrorMessage().c_str());

		uint32_t byteCodeSize = (module.cend() - module.cbegin()) * sizeof(uint32_t);
		const char* _code = reinterpret_cast<const char*>(module.cbegin());
		code = std::vector<char>(_code, _code + byteCodeSize);
	}
#endif

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	LOG_IF( (vkCreateShaderModule(a_pRenderer->device, &createInfo, nullptr, &pShaderModule->shaderModule) == VK_SUCCESS),
		LogSeverity::ERR, "failed to create shader module!" );

	free(buffer);
}

void DestroyShaderModule(Renderer* a_pRenderer, ShaderModule** a_ppShaderModule)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppShaderModule, LogSeverity::ERR, "Value at a_ppShaderModule is NULL");

	vkDestroyShaderModule(a_pRenderer->device, (*a_ppShaderModule)->shaderModule, nullptr);
	(*a_ppShaderModule)->shaderModule = VK_NULL_HANDLE;
}

uint32_t GetNextSwapchainImage(Renderer* a_pRenderer)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");

	vkWaitForFences(a_pRenderer->device, 1, &a_pRenderer->inFlightFences[a_pRenderer->currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(a_pRenderer->device, a_pRenderer->swapChain, UINT64_MAX, a_pRenderer->imageAvailableSemaphores[a_pRenderer->currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		a_pRenderer->window.pApp->Unload();
		a_pRenderer->window.pApp->Load();
		return -1;
	}
	else if (result != VK_SUCCESS)
	{
		LOG(LogSeverity::ERR, "failed to acquire swap chain image!");
	}

	a_pRenderer->imageIndex = imageIndex;

	return imageIndex;
}

#pragma region Commands

void CreateCommandPool(Renderer** a_ppRenderer)
{
	LOG_IF(*a_ppRenderer, LogSeverity::ERR, "Value at a_ppRenderer is NULL");
	Renderer* pRenderer = *a_ppRenderer;

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = familyIndices.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Optional
	LOG_IF((vkCreateCommandPool(pRenderer->device, &poolInfo, nullptr, &pRenderer->commandPool) == VK_SUCCESS), LogSeverity::ERR, "failed to create command pool!");
}

void CreateCommandBuffers(Renderer* a_pRenderer, uint32_t a_uiCount, CommandBuffer** a_ppCommandBuffers)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");
	LOG_IF(*a_ppCommandBuffers, LogSeverity::ERR, "Value at a_ppCommandBuffers is NULL");

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = a_pRenderer->commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)a_uiCount;

	std::vector<VkCommandBuffer> cmdBuffers(a_uiCount);
	LOG_IF((vkAllocateCommandBuffers(a_pRenderer->device, &allocInfo, cmdBuffers.data()) == VK_SUCCESS), LogSeverity::ERR, "failed to allocate command buffers!");

	for (uint32_t i = 0; i < a_pRenderer->maxInFlightFrames; ++i)
	{
		a_ppCommandBuffers[i]->commandBuffer = cmdBuffers[i];
		a_ppCommandBuffers[i]->pRenderer = a_pRenderer;
	}
}

void DestroyCommandBuffers(Renderer* a_pRenderer, uint32_t a_uiCount, CommandBuffer** a_ppCommandBuffers)
{
	LOG_IF(a_pRenderer, LogSeverity::ERR, "a_pRenderer is NULL");

	std::vector<VkCommandBuffer> cmds(a_uiCount);
	for (uint32_t i=0; i < a_uiCount; ++i)
	{
		cmds[i] = a_ppCommandBuffers[i]->commandBuffer;
	}

	vkFreeCommandBuffers(a_pRenderer->device, a_pRenderer->commandPool, static_cast<uint32_t>(cmds.size()), cmds.data());
}

void BeginCommandBuffer(CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "a_pCommandBuffer is NULL");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;
	LOG_IF(vkBeginCommandBuffer(a_pCommandBuffer->commandBuffer , &beginInfo) == VK_SUCCESS, LogSeverity::ERR, "Couldn't begin command buffer recording");
}

void EndCommandBuffer(CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	
	if (a_pCommandBuffer->activeRenderPass != VK_NULL_HANDLE)
	{
		vkCmdEndRenderPass(a_pCommandBuffer->commandBuffer);
	}

	LOG_IF(vkEndCommandBuffer(a_pCommandBuffer->commandBuffer) == VK_SUCCESS, LogSeverity::ERR, "Couldn't end command buffer recording");
	a_pCommandBuffer->activeRenderPass = VK_NULL_HANDLE;
}

void SetViewport(CommandBuffer* a_pCommandBuffer, float a_fX, float a_fY, float a_fWidth, float a_fHeight, float a_fMinDepth, float a_fMaxDepth)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");

	VkViewport viewport;
	viewport.x = a_fX;
	viewport.y = a_fX;
	viewport.width = a_fWidth;
	viewport.height = a_fHeight;
	viewport.minDepth = a_fMinDepth;
	viewport.maxDepth = a_fMaxDepth;
	vkCmdSetViewport(a_pCommandBuffer->commandBuffer, 0, 1, &viewport);
}

void SetScissors(CommandBuffer* a_pCommandBuffer, uint32_t a_uX, uint32_t a_uY, uint32_t a_uWidth, uint32_t a_uHeight)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");

	VkRect2D rect;
	rect.offset.x = a_uX;
	rect.offset.y = a_uY;
	rect.extent.width = a_uWidth;
	rect.extent.height = a_uHeight;
	vkCmdSetScissor(a_pCommandBuffer->commandBuffer, 0, 1, &rect);
}

void BindPipeline(CommandBuffer* a_pCommandBuffer, Pipeline* a_pPipeline)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	LOG_IF(a_pPipeline, LogSeverity::ERR, "a_pPipeline is NULL");

	vkCmdBindPipeline(a_pCommandBuffer->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, a_pPipeline->pipeline);
}

void Draw(CommandBuffer* a_pCommandBuffer, uint32_t a_uVertexCount, uint32_t a_uFirstVertex)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	
	vkCmdDraw(a_pCommandBuffer->commandBuffer, a_uVertexCount, 1, a_uFirstVertex, 0);
}

void Submit(CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	LOG_IF(a_pCommandBuffer->pRenderer, LogSeverity::ERR, "pRenderer is NULL");
	LOG_IF(a_pCommandBuffer->activeRenderPass == VK_NULL_HANDLE, LogSeverity::ERR, "Submitting when in active render pass!");

	Renderer* pRenderer = a_pCommandBuffer->pRenderer;

	VkSemaphore waitSemaphores[] = { pRenderer->imageAvailableSemaphores[pRenderer->currentFrame] };
	VkSemaphore signalSemaphores[] = { pRenderer->renderFinishedSemaphores[pRenderer->currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &(a_pCommandBuffer->commandBuffer);

	vkResetFences(pRenderer->device, 1, &(pRenderer->inFlightFences[pRenderer->currentFrame]));

	LOG_IF( (vkQueueSubmit(pRenderer->graphicsQueue, 1, &submitInfo, pRenderer->inFlightFences[pRenderer->currentFrame]) == VK_SUCCESS), LogSeverity::ERR, "failed to submit to queue!");
}

void Present(CommandBuffer* a_pCommandBuffer)
{
	LOG_IF(a_pCommandBuffer, LogSeverity::ERR, "a_pCommandBuffer is NULL");
	LOG_IF(a_pCommandBuffer->commandBuffer != VK_NULL_HANDLE, LogSeverity::ERR, "command buffer is VK_NULL_HANDLE");
	LOG_IF(a_pCommandBuffer->pRenderer, LogSeverity::ERR, "pRenderer is NULL");
	LOG_IF(a_pCommandBuffer->activeRenderPass == VK_NULL_HANDLE, LogSeverity::ERR, "Presenting when in active render pass!");

	Renderer* pRenderer = a_pCommandBuffer->pRenderer;
	
	VkSemaphore signalSemaphores[] = { pRenderer->renderFinishedSemaphores[pRenderer->currentFrame] };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { pRenderer->swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &pRenderer->imageIndex;
	presentInfo.pResults = nullptr; // Optional

	VkResult result = vkQueuePresentKHR(pRenderer->presentQueue, &presentInfo);
	LOG_IF( (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR), LogSeverity::ERR,"failed to present swap chain image!");

	// Note:
	// On android, using IDENTITY preTransform gives VK_SUBOPTIMAL_KHR result on present, so the check is removed here
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		a_pCommandBuffer->pRenderer->window.pApp->Unload();
		a_pCommandBuffer->pRenderer->window.pApp->Load();
	}

	pRenderer->currentFrame = (pRenderer->currentFrame + 1) % pRenderer->maxInFlightFrames;
}

#pragma endregion

#pragma endregion
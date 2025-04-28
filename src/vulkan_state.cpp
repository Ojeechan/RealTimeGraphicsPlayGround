#include "vulkan_state.hpp"

#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "window_state.hpp"
#include "camera.hpp"
#include "game_object.hpp"
#include "vulkan_utils.hpp"
#include "vulkan_types.hpp"
#include "forward_renderpass.hpp"
#include "deferred_renderpass.hpp"
#include "pixel_renderpass.hpp"
#include "raytracing_pipeline.hpp"
#include "constants.hpp"
#include "buffer_types.hpp"

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> rtExtensions = {
	VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
	VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::unordered_map<std::string, int> VulkanState::textureTypeMap = {
	{"albedo", 0},
	{"normal", 1},
	{"material", 2}
};

enum TEXTURE_TYPES {
	ALBEDO = 0,
	NORMAL = 1,
	MATERIAL = 2
};

void VulkanState::changeRenderPass() {
	gui.proceedRenderModeIndex();
	shouldSwitchRenderPass = true;
}

void VulkanState::init() {
	createCommonResource();
	gui.init(
		windowState.getWindow(),
		instance,
		physicalDevice,
		device,
		findQueueFamilies(physicalDevice).graphicsFamily.value(),
		graphicsQueue,
		swapchain
	);
	gui.setRenderModeChangedCallback([this]() {
		switchRenderPassCallback();
	});
	swapchainRenderPass = std::make_unique<SwapchainRenderPass>(physicalDevice, device, swapchain, graphicsQueue, commandPool);
	swapchainRenderPass->init();
}

void VulkanState::switchRenderPassCallback() {
	shouldSwitchRenderPass = true;
}

void VulkanState::createCommonResource() {
	createInstance();
	setupDebugMessenger();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createSwapchainImageViews();
	createCommandPool();
	createCommandBuffers();
	createTextureSampler();
	createSyncObjects();
}

void VulkanState::createRenderModeResource() {
	if (gui.isRayTracingMode()) {
		return;
	}
	if (renderModeManager != nullptr) {
    	oldRenderPassQueue[currentFrame].push_back(std::move(renderModeManager));
	}
	switch (gui.getMode()) {
		case 1: {
			auto renderPass = std::make_unique<DeferredRenderPass>(
				physicalDevice,
				device,
				commonDescriptor,
				modelTextureDescriptorSetLayout,
				swapchain,
				VulkanUtils::findDepthFormat(physicalDevice)
			);
			renderModeManager = std::move(renderPass);
			break;
		}
		case 2: {
			auto renderPass = std::make_unique<PixelRenderPass>(
				physicalDevice,
				device,
				commonDescriptor,
				modelTextureDescriptorSetLayout,
				swapchain,
				VulkanUtils::findDepthFormat(physicalDevice)
			);
			renderModeManager = std::move(renderPass);
			break;
		}
		default:
			auto renderPass = std::make_unique<ForwardRenderPass>(
				physicalDevice,
				device,
				commonDescriptor,
				modelTextureDescriptorSetLayout,
				swapchain,
				VulkanUtils::findDepthFormat(physicalDevice),
				getMaxUsableSampleCount(),
				swapchainRenderPass->getRenderTargetResource()
			);
			renderModeManager = std::move(renderPass);
			break;
	}
	renderModeManager->init();
}

void VulkanState::cleanupRenderModeResource() {
	if (shouldSwitchRenderPass) {
		createRenderModeResource();
		shouldSwitchRenderPass = false;
	}
}

void VulkanState::updateLightSSBO(std::vector<PointLightBuffer>& pointLights, std::vector<DirectionalLightBuffer>& directionalLights) {
	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		memcpy(pointLightSSBOResource.buffersMapped[i], pointLights.data(), sizeof(PointLightBuffer) * pointLights.size());
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		memcpy(directionalLightSSBOResource.buffersMapped[i], directionalLights.data(), sizeof(DirectionalLightBuffer) * directionalLights.size());
	}
}

void VulkanState::createLevelResource(size_t modelCount, size_t pointLightCount, size_t dirLightCount) {
	createBufferResource(sizeof(TransformMatrixBuffer) * modelCount, modelMatrixUBOResource, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBufferResource(sizeof(CameraMatrixBuffer), cameraMatrixUBOResource, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBufferResource(sizeof(CameraBuffer), cameraUBOResource, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	createBufferResource(sizeof(PointLightBuffer) * pointLightCount, pointLightSSBOResource, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	createBufferResource(sizeof(DirectionalLightBuffer) * dirLightCount, directionalLightSSBOResource, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	createModelDescriptorPool(modelCount, pointLightCount + dirLightCount);
	createModelMatrixUBODescriptor();
	createCameraMatrixUBODescriptor();
	createCameraUBODescriptor();
	createLightSSBODescriptor(pointLightCount, dirLightCount);
	createModelTextureDescriptorSetLayout();
	createRenderModeResource();

	// for debugging purpose
	if (gui.isRayTracingAvailable()) {
		rayTracingPipeline = std::make_unique<RayTracingPipeline>(
			physicalDevice,
			device,
			commonDescriptor,
			commandPool,
			graphicsQueue,
			swapchainRenderPass->getRenderTargetResource()
		);
		rayTracingPipeline->init();
	}
}

void VulkanState::cleanupSwapchain() {
	for (auto imageView : swapchain.imageViews) {
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
}

void VulkanState::cleanup(AssetData& player, std::vector<AssetData>& props) {
	gui.cleanup(device);
	rayTracingPipeline->cleanup();
	swapchainRenderPass->cleanup();

	player.resource.cleanup(device);
	for (auto& prop : props) {
		prop.resource.cleanup(device);
	}

	renderModeManager->cleanup();
	commonDescriptor.cleanup(device);
	cleanupSwapchain();

	modelMatrixUBOResource.cleanup(device);
	cameraMatrixUBOResource.cleanup(device);
	cameraUBOResource.cleanup(device);
	pointLightSSBOResource.cleanup(device);
	directionalLightSSBOResource.cleanup(device);

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}

	vkDestroyDescriptorSetLayout(device, modelTextureDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(device, modelDescriptorPool, nullptr);
	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, nullptr);

	if (enableValidationLayers) {
		VulkanUtils::destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void VulkanState::recreateSwapchain() {
	int width = 0, height = 0;
	glfwGetFramebufferSize(windowState.getWindow(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(windowState.getWindow(), &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);
	renderModeManager->cleanupImageResources();
	swapchainRenderPass->cleanupImageResources();
	cleanupSwapchain();

	createSwapchain();
	createSwapchainImageViews();

	renderModeManager->setSwapchain(swapchain);
	renderModeManager->createImageResources();
	swapchainRenderPass->setSwapchain(swapchain);
	swapchainRenderPass->createImageResources();

	gui.recreateFramebuffer(device, swapchain);
}

void VulkanState::createInstance() {
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested");
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Rendering Subsystem";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
	} else {
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance");
	}
}

void VulkanState::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanState::setupDebugMessenger() {
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	if (VulkanUtils::createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger");
	}
}

void VulkanState::createSurface() {
	if (glfwCreateWindowSurface(instance, windowState.getWindow(), nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface");
	}
}

void VulkanState::pickPhysicalDevice() {
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("no GPUs with Vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			physicalDevice = device;
		    break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("no suitable GPU");
	}
}

void VulkanState::createLogicalDevice() {
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures basicFeatures{};
	basicFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	createInfo.pEnabledFeatures = &basicFeatures;

	VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
	bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
	bufferDeviceAddressFeatures.pNext = nullptr;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
	rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rtPipelineFeatures.rayTracingPipeline = VK_TRUE;
	rtPipelineFeatures.pNext = &bufferDeviceAddressFeatures;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
	asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	asFeatures.accelerationStructure = VK_TRUE;
	asFeatures.pNext = &rtPipelineFeatures;

	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &asFeatures;
	deviceFeatures2.features = basicFeatures;

	std::vector<const char*> requiredExtensions = deviceExtensions;
	if (gui.isRayTracingAvailable()) {
		requiredExtensions.insert(requiredExtensions.end(), rtExtensions.begin(), rtExtensions.end());
		createInfo.pNext = &deviceFeatures2;
		createInfo.pEnabledFeatures = nullptr;
	}

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	} else {
		createInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
		throw std::runtime_error("failed to create logical device");
	}

	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanState::createSwapchain() {
	SwapchainSupportDetails swapchainSupport = querySwapchainSupport(physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

	uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
	if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
		imageCount = swapchainSupport.capabilities.maxImageCount;
	}

	swapchain.minImageCount = imageCount;

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;

	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain.handle) != VK_SUCCESS) {
		throw std::runtime_error("failed to create swapchain");
	}

	vkGetSwapchainImagesKHR(device, swapchain.handle, &imageCount, nullptr);
	swapchain.images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain.handle, &imageCount, swapchain.images.data());

	swapchain.imageFormat = surfaceFormat.format;
	swapchain.extent = extent;
}

void VulkanState::createSwapchainImageViews() {
	swapchain.imageViews.resize(swapchain.images.size());

	for (uint32_t i = 0; i < swapchain.images.size(); ++i) {
		swapchain.imageViews[i] = VulkanUtils::createImageView(device, swapchain.images[i], swapchain.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void VulkanState::createCommandPool() {
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create command pool");
	}
}

void VulkanState::createTextureImage(std::string path, VkImage& image, VkDeviceMemory& imageMemory) {
	int textureWidth, textureHeight, textureChannels;
	stbi_uc* pixels = stbi_load(
		path.c_str(),
		&textureWidth,
		&textureHeight,
		&textureChannels,
		STBI_rgb_alpha
	);
	VkDeviceSize imageSize = textureWidth * textureHeight * 4;
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max<int>(textureWidth, textureHeight)))) + 1;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory,
		nullptr
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	{
		memcpy(data, pixels, static_cast<size_t>(imageSize));
	}
	vkUnmapMemory(device, stagingBufferMemory);

	stbi_image_free(pixels);

	VulkanUtils::createImage(
		physicalDevice,
		device,
		textureWidth,
		textureHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		image,
		imageMemory
	);

	transitionImageLayout(
		image,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mipLevels
	);
	copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight));

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	generateMipmaps(image, VK_FORMAT_R8G8B8A8_SRGB, textureWidth, textureHeight, mipLevels);
}

// TODO read explanation
void VulkanState::generateMipmaps(VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels) {
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting");
	}

	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = width;
	int32_t mipHeight = height;

	for (uint32_t i = 1; i < mipLevels; ++i) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&barrier
		);

		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(
			commandBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&blit,
			VK_FILTER_LINEAR
		);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&barrier
		);

		if (mipWidth > 1) {
			mipWidth /= 2;
		}
		if (mipHeight > 1) {
			mipHeight /= 2;
		}
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier
	);

	VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
}

VkSampleCountFlagBits VulkanState::getMaxUsableSampleCount() {
	VkPhysicalDeviceProperties physicalDeviceProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

	VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & VK_SAMPLE_COUNT_64_BIT) {
		return VK_SAMPLE_COUNT_64_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_32_BIT) {
		return VK_SAMPLE_COUNT_32_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_16_BIT) {
		return VK_SAMPLE_COUNT_16_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_8_BIT) {
		return VK_SAMPLE_COUNT_8_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_4_BIT) {
		return VK_SAMPLE_COUNT_4_BIT;
	}
	if (counts & VK_SAMPLE_COUNT_2_BIT) {
		return VK_SAMPLE_COUNT_2_BIT;
	}
	return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanState::createTextureSampler() {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = static_cast<float>(mipLevels);
	createInfo.mipLodBias = 0.0f;

	if (vkCreateSampler(device, &createInfo, nullptr, &textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler");
	}
}

void VulkanState::transitionImageLayout(
	VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	uint32_t mipLevels
) {
	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;static auto startTime = std::chrono::high_resolution_clock::now();
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	} else {
		throw std::invalid_argument("unsupported layout transition");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
}

void VulkanState::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);

	VkBufferImageCopy copyRegion{};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageOffset = {0, 0, 0};
	copyRegion.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
}

ModelResource VulkanState::createModelResource(std::string textureDir, std::string modelDir, nlohmann::json data) {
	ModelResource model{};
	std::array<VkImageView, 3> textureImageViews;

	// create texture resources
	for (const auto& [key, value] : data["textures"].items()) {
		int index = textureTypeMap.at(key);
		auto& textureResource = model.textureResources[index];
		std::string filename = value;
		createTextureImage("../" + textureDir + "/" + filename, textureResource.image, textureResource.imageMemory);
		textureResource.imageView = VulkanUtils::createImageView(device, textureResource.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
		textureImageViews[index] = textureResource.imageView;
	}

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warningMessage, errorMessage;

	std::string modelName = data["model"];
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warningMessage, &errorMessage, ("../" + modelDir + "/" + modelName).c_str())) {
		throw std::runtime_error(warningMessage + errorMessage);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};
			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = {1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}

	// load vertices and create vertex buffer
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	createVertexBuffer(vertices, vertexBuffer, vertexBufferMemory);
	model.vertexBufferResource.buffer = vertexBuffer;
	model.vertexBufferResource.bufferMemory = vertexBufferMemory;

	// create index buffer
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	createIndexBuffer(indices, indexBuffer, indexBufferMemory);
	model.indexBufferResource.buffer = indexBuffer;
	model.indexBufferResource.bufferMemory = indexBufferMemory;
	model.indexCount = size(indices);

	// create DescriptorSet
	std::vector<VkDescriptorSet> descriptorSets;
	createModelTextureDescriptorSets(descriptorSets, textureImageViews);
	model.descriptorSets = descriptorSets;

	return model;
}

void VulkanState::createVertexBuffer(std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory) {
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory,
		nullptr
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	{
		memcpy(data, vertices.data(), (size_t) bufferSize);
	}
	vkUnmapMemory(device, stagingBufferMemory);

	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer,
		vertexBufferMemory,
		nullptr
	);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanState::createIndexBuffer(std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory) {
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory,
		nullptr
	);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	{
		memcpy(data, indices.data(), (size_t) bufferSize);
	}
	vkUnmapMemory(device, stagingBufferMemory);

	VulkanUtils::createBuffer(
		physicalDevice,
		device,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer,
		indexBufferMemory,
		nullptr
	);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanState::createBufferResource(VkDeviceSize bufferSize, BufferResource& bufferResource, VkBufferUsageFlags usage) {
	bufferResource.buffers.resize(Config::MAX_FRAMES_IN_FLIGHT);
	bufferResource.buffersMemory.resize(Config::MAX_FRAMES_IN_FLIGHT);
	bufferResource.buffersMapped.resize(Config::MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VulkanUtils::createBuffer(
			physicalDevice,
			device,
			bufferSize,
			usage,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			bufferResource.buffers[i],
			bufferResource.buffersMemory[i],
			nullptr
		);
	    vkMapMemory(device, bufferResource.buffersMemory[i], 0, bufferSize, 0, &bufferResource.buffersMapped[i]);
	}
}

void VulkanState::createModelDescriptorPool(size_t assetCount, size_t lightCount) {
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	uint32_t uboDescriptorSetCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT * 3);
	uint32_t ssboDescriptorSetCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT * lightCount * 2);
	uint32_t imageDescriptorSetCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT * assetCount * 5);
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = uboDescriptorSetCount;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = ssboDescriptorSetCount;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[2].descriptorCount = imageDescriptorSetCount;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();
	createInfo.maxSets = Config::MAX_FRAMES_IN_FLIGHT * 50;

	if (vkCreateDescriptorPool(device, &createInfo, nullptr, &modelDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool");
	}
}

void VulkanState::createModelTextureDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding albedoLayoutBinding{};
	albedoLayoutBinding.binding = TEXTURE_TYPES::ALBEDO;
	albedoLayoutBinding.descriptorCount = 1;
	albedoLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	albedoLayoutBinding.pImmutableSamplers = nullptr;
	albedoLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding normalLayoutBinding{};
	normalLayoutBinding.binding = TEXTURE_TYPES::NORMAL;
	normalLayoutBinding.descriptorCount = 1;
	normalLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalLayoutBinding.pImmutableSamplers = nullptr;
	normalLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding materialLayoutBinding{};
	materialLayoutBinding.binding = TEXTURE_TYPES::MATERIAL;
	materialLayoutBinding.descriptorCount = 1;
	materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	materialLayoutBinding.pImmutableSamplers = nullptr;
	materialLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 3> textureBindings = {
		albedoLayoutBinding,
		normalLayoutBinding,
		materialLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(textureBindings.size());
	createInfo.pBindings = textureBindings.data();

	if (vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &modelTextureDescriptorSetLayout) != VK_SUCCESS) {
		std::runtime_error("failed to create descriptor set layout");
	}
}

void VulkanState::createModelTextureDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, std::array<VkImageView, 3>& textureImageViews) {
	std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, modelTextureDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = modelDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		std::vector<VkDescriptorImageInfo> imageInfos;

		for (auto textureImageView : textureImageViews) {
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;
			imageInfos.push_back(imageInfo);
		}

		std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
		for (int j = 0; j < imageInfos.size(); j++) {
			descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[j].dstSet = descriptorSets[i];
			descriptorWrites[j].dstBinding = j;
			descriptorWrites[j].dstArrayElement = 0;
			descriptorWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[j].descriptorCount = 1;
			descriptorWrites[j].pImageInfo = &imageInfos[j];
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void VulkanState::createModelMatrixUBODescriptor() {
	VkDescriptorSetLayoutBinding modelMatrixLayoutBinding{};
	modelMatrixLayoutBinding.binding = 0;
	modelMatrixLayoutBinding.descriptorCount = 1;
	modelMatrixLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelMatrixLayoutBinding.pImmutableSamplers = nullptr;
	modelMatrixLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo modelMatrixLayoutCreateInfo{};
	modelMatrixLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	modelMatrixLayoutCreateInfo.bindingCount = 1;
	modelMatrixLayoutCreateInfo.pBindings = &modelMatrixLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &modelMatrixLayoutCreateInfo, nullptr, &commonDescriptor.modelMatrix.layout) != VK_SUCCESS) {
		std::runtime_error("failed to create descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, commonDescriptor.modelMatrix.layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = modelDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	commonDescriptor.modelMatrix.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, commonDescriptor.modelMatrix.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo modelMatrixBufferInfo{};
		modelMatrixBufferInfo.buffer = modelMatrixUBOResource.buffers[i];
		modelMatrixBufferInfo.offset = 0;
		modelMatrixBufferInfo.range = sizeof(TransformMatrixBuffer);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = commonDescriptor.modelMatrix.sets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &modelMatrixBufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

void VulkanState::createCameraMatrixUBODescriptor() {
	VkDescriptorSetLayoutBinding cameraMatrixLayoutBinding{};
	cameraMatrixLayoutBinding.binding = 0;
	cameraMatrixLayoutBinding.descriptorCount = 1;
	cameraMatrixLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraMatrixLayoutBinding.pImmutableSamplers = nullptr;
	cameraMatrixLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutCreateInfo cameraLayoutCreateInfo{};
	cameraLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	cameraLayoutCreateInfo.bindingCount = 1;
	cameraLayoutCreateInfo.pBindings = &cameraMatrixLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &cameraLayoutCreateInfo, nullptr, &commonDescriptor.cameraMatrix.layout) != VK_SUCCESS) {
		std::runtime_error("failed to create descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, commonDescriptor.cameraMatrix.layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = modelDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	commonDescriptor.cameraMatrix.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, commonDescriptor.cameraMatrix.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo cameraMatrixBufferInfo{};
		cameraMatrixBufferInfo.buffer = cameraMatrixUBOResource.buffers[i];
		cameraMatrixBufferInfo.offset = 0;
		cameraMatrixBufferInfo.range = sizeof(CameraMatrixBuffer);

		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = commonDescriptor.cameraMatrix.sets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &cameraMatrixBufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

void VulkanState::createCameraUBODescriptor() {
	VkDescriptorSetLayoutBinding cameraLayoutBinding{};
	cameraLayoutBinding.binding = 0;
	cameraLayoutBinding.descriptorCount = 1;
	cameraLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraLayoutBinding.pImmutableSamplers = nullptr;
	cameraLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutCreateInfo cameraLayoutCreateInfo{};
	cameraLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	cameraLayoutCreateInfo.bindingCount = 1;
	cameraLayoutCreateInfo.pBindings = &cameraLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &cameraLayoutCreateInfo, nullptr, &commonDescriptor.camera.layout) != VK_SUCCESS) {
		std::runtime_error("failed to create descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, commonDescriptor.camera.layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = modelDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	commonDescriptor.camera.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, commonDescriptor.camera.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo cameraBufferInfo{};
		cameraBufferInfo.buffer = cameraUBOResource.buffers[i];
		cameraBufferInfo.offset = 0;
		cameraBufferInfo.range = sizeof(CameraBuffer);

		VkWriteDescriptorSet descriptorWrite;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = commonDescriptor.camera.sets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &cameraBufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

void VulkanState::createLightSSBODescriptor(size_t pointLightCount, size_t dirLightCount) {
	std::array<VkDescriptorSetLayoutBinding, 2> lightLayoutBindings{};
	lightLayoutBindings[0].binding = 0;
	lightLayoutBindings[0].descriptorCount = 1;
	lightLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightLayoutBindings[0].pImmutableSamplers = nullptr;
	lightLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	lightLayoutBindings[1].binding = 1;
	lightLayoutBindings[1].descriptorCount = 1;
	lightLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightLayoutBindings[1].pImmutableSamplers = nullptr;
	lightLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo lightLayoutCreateInfo{};
	lightLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	lightLayoutCreateInfo.bindingCount = static_cast<uint32_t>(lightLayoutBindings.size());
	lightLayoutCreateInfo.pBindings = lightLayoutBindings.data();

	if (vkCreateDescriptorSetLayout(device, &lightLayoutCreateInfo, nullptr, &commonDescriptor.light.layout) != VK_SUCCESS) {
		std::runtime_error("failed to create descriptor set layout");
	}

	std::vector<VkDescriptorSetLayout> layouts(Config::MAX_FRAMES_IN_FLIGHT, commonDescriptor.light.layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = modelDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(Config::MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	commonDescriptor.light.resize(Config::MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &allocInfo, commonDescriptor.light.sets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor set");
	}

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo pointLightBufferInfo{};
		pointLightBufferInfo.buffer = pointLightSSBOResource.buffers[i];
		pointLightBufferInfo.offset = 0;
		pointLightBufferInfo.range = sizeof(PointLightBuffer) * pointLightCount;

		VkDescriptorBufferInfo directionalLightBufferInfo{};
		directionalLightBufferInfo.buffer = directionalLightSSBOResource.buffers[i];
		directionalLightBufferInfo.offset = 0;
		directionalLightBufferInfo.range = sizeof(DirectionalLightBuffer) * dirLightCount;


		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = commonDescriptor.light.sets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &pointLightBufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = commonDescriptor.light.sets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &directionalLightBufferInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
}

void VulkanState::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBuffer commandBuffer = VulkanUtils::beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	VulkanUtils::endSingleTimeCommands(device, commandPool, commandBuffer, graphicsQueue);
}

void VulkanState::createCommandBuffers() {
	commandBuffers.resize(Config::MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers");
	}
}

void VulkanState::createSyncObjects() {
	imageAvailableSemaphores.resize(Config::MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(Config::MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(Config::MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
		    || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
		    || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create sync objects for a frame");
		}
	}
}

void VulkanState::updateCamera(const Camera& camera) {
	CameraBuffer cameraUBO{};
	cameraUBO.position = camera.getPosition();
	cameraUBO.front = camera.getFront();
	cameraUBO.up = camera.getUp();
	cameraUBO.fov = camera.getFOV();

	memcpy(cameraUBOResource.buffersMapped[currentFrame], &cameraUBO, sizeof(cameraUBO));

	CameraMatrixBuffer cameraMatrixUBO{};
	cameraMatrixUBO.view = camera.getViewMatrix();

	int width, height;
	glfwGetFramebufferSize(windowState.getWindow(), &width, &height);
	float aspect = (float)width / height;
	cameraMatrixUBO.projection = camera.isPerspective()
	    ? glm::perspective(camera.getFOV(), aspect, camera.getNearPlane(), camera.getFarPlane())
		: glm::ortho(-aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
	cameraMatrixUBO.projection[1][1] *= -1;

	memcpy(cameraMatrixUBOResource.buffersMapped[currentFrame], &cameraMatrixUBO, sizeof(cameraMatrixUBO));
}

void VulkanState::render(
	const std::vector<AssetData>& objects,
	const Camera& camera,
	const std::vector<DirectionalLightBuffer> directionalLights
) {
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		device,
		swapchain.handle,
		UINT64_MAX,
		imageAvailableSemaphores[currentFrame],
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swapchain image");
	}

	vkResetFences(device, 1, &inFlightFences[currentFrame]);

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer");
	}

	if (gui.isRayTracingMode()) {
		rayTracingPipeline->render(commandBuffers[currentFrame], imageIndex, currentFrame, camera, directionalLights, swapchain.extent);
		swapchainRenderPass->render(commandBuffers[currentFrame], imageIndex, currentFrame);
	} else {
		renderModeManager->render(
			commandBuffers,
			imageIndex,
			currentFrame,
			modelMatrixUBOResource.buffersMapped,
			objects,
			camera,
			directionalLights,
			windowState.getWindow()
		);
	}

	gui.render(commandBuffers[currentFrame], swapchain.extent, imageIndex);

	if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer");
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = {swapchain.handle};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowState.isFramebufferResized()) {
		windowState.setFramebufferResized(false);
		recreateSwapchain();
	} else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swapchain image");
	}

	currentFrame = (currentFrame + 1) % Config::MAX_FRAMES_IN_FLIGHT;

	for (auto& oldRenderPassQueue : oldRenderPassQueue[currentFrame]) {
		oldRenderPassQueue->cleanup();
	}
	oldRenderPassQueue[currentFrame].clear();
}

VkSurfaceFormatKHR VulkanState::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanState::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanState::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	} else {
		int width, height;
		glfwGetFramebufferSize(windowState.getWindow(), &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

SwapchainSupportDetails VulkanState::querySwapchainSupport(const VkPhysicalDevice& device) {
	SwapchainSupportDetails details;

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

bool VulkanState::isDeviceSuitable(const VkPhysicalDevice& device) {
	QueueFamilyIndices indices = findQueueFamilies(device);
	bool extensionsSupported = checkDeviceExtensionSupport(device);
	bool swapchainAdequate = false;
	if (extensionsSupported) {
		SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
		swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
	}
	VkPhysicalDeviceFeatures supportedFeatures;
	vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.isComplete() && extensionsSupported && swapchainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanState::checkDeviceExtensionSupport(const VkPhysicalDevice& device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	std::set<std::string> requiredRTExtensions(rtExtensions.begin(), rtExtensions.end());

	for (const auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
		requiredRTExtensions.erase(extension.extensionName);
	}

	gui.setRayTracingAvailable(requiredRTExtensions.empty());
	return requiredExtensions.empty();
}

QueueFamilyIndices VulkanState::findQueueFamilies(const VkPhysicalDevice& device) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		++i;
	}

	return indices;
}

std::vector<const char*> VulkanState::getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

bool VulkanState::checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}
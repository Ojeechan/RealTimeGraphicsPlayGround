#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <nlohmann/json.hpp>

#include <optional>
#include <fstream>
#include <iostream>

#include "vulkan_types.hpp"
#include "vulkan_vertex.hpp"
#include "gui_renderpass.hpp"
#include "base_renderpass.hpp"
#include "constants.hpp"
#include "swapchain_renderpass.hpp"
#include "raytracing_pipeline.hpp"

class Camera;
class WindowState;
class PointLightBuffer;
class DirectionalLightBuffer;

class VulkanState {
public:
    VulkanState(WindowState& windowState) : windowState(windowState) {};
	~VulkanState() = default;
	void init();
	void createCommonResource();
	void createRenderModeResource();
	void cleanupRenderModeResource();
	void createLevelResource(size_t modelCount, size_t pointLightCount, size_t dirLightCount);
	ModelResource createModelResource(std::string textureDir, std::string modelDir, nlohmann::json data);
	void updateLightSSBO(std::vector<PointLightBuffer>& pointLights, std::vector<DirectionalLightBuffer>& directionalLights);
	void createModelDescriptorPool(size_t modelCount, size_t lightCount);
	void updateCamera(const Camera& camera);
	void render(
		const std::vector<AssetData>& objects,
		const Camera& camera,
		const std::vector<DirectionalLightBuffer> directionalLights
	);
	void cleanup(AssetData& player, std::vector<AssetData>& props);
	void deviceWaitIdle() {
		vkDeviceWaitIdle(device);
	}
	void changeRenderPass();
	static const std::unordered_map<std::string, int> textureTypeMap;

private:
	VkInstance instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;

	Swapchain swapchain;
	std::unique_ptr<SwapchainRenderPass> swapchainRenderPass;
	std::unique_ptr<RayTracingPipeline> rayTracingPipeline;

	VkDescriptorPool modelDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout modelTextureDescriptorSetLayout = VK_NULL_HANDLE;
	CommonDescriptor commonDescriptor;

	BufferResource modelMatrixUBOResource;
	BufferResource cameraMatrixUBOResource;
	BufferResource cameraUBOResource;
	BufferResource pointLightSSBOResource;
	BufferResource directionalLightSSBOResource;

	VkSampler textureSampler = VK_NULL_HANDLE;

	std::unique_ptr<BaseRenderPass> renderModeManager;
	std::vector<std::unique_ptr<BaseRenderPass>> oldRenderPassQueue[Config::MAX_FRAMES_IN_FLIGHT];

	VkCommandPool commandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;

	WindowState& windowState;
	VulkanGUI gui;
	bool shouldSwitchRenderPass = false;

	uint32_t mipLevels = 1;
	uint32_t currentFrame = 0;

	void createInstance();
	void setupDebugMessenger();
	void createSurface();
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createSwapchain();
	void createSwapchainImageViews();

	void createModelTextureDescriptorSetLayout();
	void createModelMatrixUBODescriptor();
	void createCameraMatrixUBODescriptor();
	void createCameraUBODescriptor();
	void createLightSSBODescriptor(size_t pointLightCount, size_t dirLightCount);

	void createCommandPool();
	void createCommandBuffers();
	void createTextureSampler();
	void createSyncObjects();

	void createTextureImage(std::string path,  VkImage& image, VkDeviceMemory& memory);
	void createVertexBuffer(std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);
	void createIndexBuffer(std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory);
	void createBufferResource(VkDeviceSize bufferSize, BufferResource& bufferResource, VkBufferUsageFlags usage);
	void createModelTextureDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, std::array<VkImageView, 3>& textureImageViews);

	bool checkValidationLayerSupport();
	std::vector<const char*> getRequiredExtensions();
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device);
	bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
	SwapchainSupportDetails querySwapchainSupport(const VkPhysicalDevice& device);
	bool isDeviceSuitable(const VkPhysicalDevice& device);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void generateMipmaps(VkImage image, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	void recreateSwapchain();
	void cleanupSwapchain();
	VkSampleCountFlagBits getMaxUsableSampleCount();
	void switchRenderPassCallback();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData
	) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}
};
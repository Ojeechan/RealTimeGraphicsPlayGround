#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>

#include "vulkan_types.hpp"
#include "camera.hpp"
#include "buffer_types.hpp"

class BaseRenderPass {
public:
	BaseRenderPass(
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		CommonDescriptor& commonDescriptor,
		VkDescriptorSetLayout modelTextureDescriptorSetLayout,
		Swapchain& swapchain,
		VkFormat depthFormat
	) : physicalDevice(physicalDevice),
		device(device),
		commonDescriptor(commonDescriptor),
		modelTextureDescriptorSetLayout(modelTextureDescriptorSetLayout),
		swapchain(swapchain),
		depthFormat(depthFormat) {};
	virtual ~BaseRenderPass() = default;
	virtual void init() = 0;
	virtual void cleanup() = 0;
	virtual void createImageResources() = 0;
	virtual void cleanupImageResources() = 0;
	virtual void render(
		std::vector<VkCommandBuffer>& commandBuffer,
		uint32_t imageIndex,
		uint32_t currentFrame,
		std::vector<void*>& modelMatrixBuffersMapped,
		const std::vector<AssetData>& assets,
		const Camera& camera,
		const std::vector<DirectionalLightBuffer>& directionalLights,
		GLFWwindow* window
	) = 0;
	void inline setSwapchain(Swapchain& swapchain) {
		this->swapchain = swapchain;
	}
protected:
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	CommonDescriptor& commonDescriptor;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkDescriptorSetLayout modelTextureDescriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers;
	Swapchain& swapchain;
	VkFormat depthFormat;
};

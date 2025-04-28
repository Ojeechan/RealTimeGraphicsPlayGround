#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <stdexcept>
#include <memory>
#include <array>

#include "vulkan_types.hpp"
#include "constants.hpp"
#include "vulkan_utils.hpp"

class SwapchainRenderPass {
public:
    SwapchainRenderPass(VkPhysicalDevice physicalDevice, VkDevice device, Swapchain& swapchain, VkQueue graphicsQueue, VkCommandPool commandPool)
		: physicalDevice(physicalDevice), device(device), swapchain(swapchain), graphicsQueue(graphicsQueue), commandPool(commandPool) {}
    ~SwapchainRenderPass() = default;
    void init();
	void cleanup();
    void cleanupImageResources();
	void createImageResources();
	void render(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t currentFrame);
	inline const Descriptor& getRenderTargetResource() const {
		return renderedImageDescriptor;
	}
	void inline setSwapchain(Swapchain& swapchain) {
		this->swapchain = swapchain;
	}
private:
	void createRenderedImage();
	void createDescriptorPool();
	void createDescriptor();
	void createSampler();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();

	VkPhysicalDevice physicalDevice;
	VkDevice device;
    Swapchain& swapchain;
	VkQueue graphicsQueue;
	VkCommandPool commandPool;

    VkRenderPass renderPass;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
	std::vector<VkFramebuffer> framebuffers;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	Descriptor renderedImageDescriptor;
	Descriptor samplerDescriptor;

	ImageResource renderedImageResource;
	VkSampler sampler = VK_NULL_HANDLE;
};

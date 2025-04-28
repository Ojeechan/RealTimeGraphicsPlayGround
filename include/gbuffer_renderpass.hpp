#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <vector>
#include <memory>

#include "vulkan_types.hpp"

class GLFWwindow;
class Camera;

class GBufferRenderPass {
public:
	GBufferRenderPass(
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
	~GBufferRenderPass() = default;

	void init();
	void createImageResources();
	void cleanup();
	void cleanupImageResources();
	void generateGBuffer(
		std::vector<VkCommandBuffer>& commandBuffers,
		uint32_t imageIndex,
		uint32_t currentFrame,
		std::vector<void*>& modelMatrixBuffersMapped,
		const std::vector<AssetData>& models
	);
	inline VkDescriptorSetLayout getGBufferLayout() {
		return descriptor.layout;
	}
	inline std::vector<VkDescriptorSet> getGBuffer() {
		return descriptor.sets;
	}

private:
	void createRenderPass();
	void createGBufferResources();
	void createFramebuffers();
	void createGraphicsPipeline();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();
	void createSampler();
	void transitionGBufferToSampler(VkCommandBuffer commandBuffer);
	void transitionGBufferToAttachment(VkCommandBuffer commandBuffer);

    VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkSampler sampler;

	ImageResource albedo;
	ImageResource position;
	ImageResource normal;
	ImageResource material;
	ImageResource depth;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	Descriptor descriptor;

	CommonDescriptor& commonDescriptor;
	VkDescriptorSetLayout modelTextureDescriptorSetLayout = VK_NULL_HANDLE;
	VkFormat depthFormat;
	Swapchain& swapchain;

	bool isTransitioned = false;
};
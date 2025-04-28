#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base_renderpass.hpp"
#include "vulkan_types.hpp"
#include "shadowmapping_renderpass.hpp"

class DeferredRenderPass : public BaseRenderPass {
public:
	DeferredRenderPass(
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		CommonDescriptor& commonDescriptor,
		VkDescriptorSetLayout modelTextureDescriptorSetLayout,
		Swapchain& swapchain,
		VkFormat depthFormat
	) : BaseRenderPass(
		physicalDevice,
		device,
		commonDescriptor,
		modelTextureDescriptorSetLayout,
		swapchain,
		depthFormat
	), shadowPass(std::make_unique<BaseShadowRenderPass>(physicalDevice, device, commonDescriptor)) {};
	~DeferredRenderPass() override = default;
	void init() override;
	void cleanup() override;
	void createImageResources() override;
	void cleanupImageResources() override;
	void render(
		std::vector<VkCommandBuffer>& commandBuffer,
		uint32_t imageIndex,
		uint32_t currentFrame,
		std::vector<void*>& modelMatrixBuffersMapped,
		const std::vector<AssetData>& assets,
		const Camera& camera,
		const std::vector<DirectionalLightBuffer>& directionalLights,
		GLFWwindow* window
	) override;
private:
	void createRenderPass();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();
	void createGBuffers();
	void createFramebuffers();
	void createGraphicsPipeline();

	ImageResource albedo;
	ImageResource position;
	ImageResource normal;
	ImageResource material;
	ImageResource depth;
	ImageResource ssao;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	Descriptor ssaoDescriptor;
	Descriptor lightingDescriptor;

	VkPipeline gBufferPipeline = VK_NULL_HANDLE;
	VkPipeline ssaoPipeline = VK_NULL_HANDLE;
	VkPipeline lightingPipeline = VK_NULL_HANDLE;

	VkPipelineLayout gBufferPipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout ssaoPipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout lightingPipelineLayout = VK_NULL_HANDLE;

	std::unique_ptr<BaseShadowRenderPass> shadowPass = nullptr;
};

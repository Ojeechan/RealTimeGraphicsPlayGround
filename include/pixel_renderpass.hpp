#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base_renderpass.hpp"
#include "gbuffer_renderpass.hpp"

class PixelRenderPass : public BaseRenderPass {
public:
	PixelRenderPass(
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
	), gBuffer(std::make_unique<GBufferRenderPass>(
		physicalDevice,
		device,
		commonDescriptor,
		modelTextureDescriptorSetLayout,
		swapchain,
		depthFormat
	)) {};
	~PixelRenderPass() override = default;
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
	void createFramebuffers();
	void createGraphicsPipeline();

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	std::unique_ptr<GBufferRenderPass> gBuffer;
};
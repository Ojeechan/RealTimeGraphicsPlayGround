#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "base_renderpass.hpp"

class ForwardRenderPass : public BaseRenderPass {
public:
	ForwardRenderPass(
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		CommonDescriptor& commonDescriptor,
		VkDescriptorSetLayout modelTextureDescriptorSetLayout,
		Swapchain& swapchain,
		VkFormat depthFormat,
		VkSampleCountFlagBits msaaSamples,
		const Descriptor& output
	) : BaseRenderPass(
		physicalDevice,
		device,
		commonDescriptor,
		modelTextureDescriptorSetLayout,
		swapchain,
		depthFormat
	), msaaSamples(msaaSamples), output(output) {};
	~ForwardRenderPass() override = default;
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
	void createColorResources();
	void createDepthResources();
	void createFramebuffers();
	void createGraphicsPipeline();

	VkImage colorImage = VK_NULL_HANDLE;
	VkDeviceMemory colorImageMemory = VK_NULL_HANDLE;
	VkImageView colorImageView = VK_NULL_HANDLE;

	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

	const Descriptor& output;
};
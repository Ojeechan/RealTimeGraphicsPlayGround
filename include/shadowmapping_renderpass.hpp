#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <vector>

#include "vulkan_types.hpp"

class Camera;

// TODO create subclass based on the shadowing technique
class BaseShadowRenderPass {
public:
	static constexpr uint32_t SHADOW_MAP_RESOLUTION = 2048;
	BaseShadowRenderPass(
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		CommonDescriptor& commonDescriptor
	): physicalDevice(physicalDevice), device(device), commonDescriptor(commonDescriptor) {};
	~BaseShadowRenderPass() = default;
	void init();
	void cleanup();
	void generateShadowMap(
		std::vector<VkCommandBuffer>& commandBuffers,
		uint32_t imageIndex,
		uint32_t currentFrame,
		std::vector<void*>& modelMatrixBuffersMapped,
		const std::vector<AssetData>& models,
		const Camera& camera,
		const std::vector<DirectionalLightBuffer>& directionalLights,
		GLFWwindow* window
	);
	void resetLayout(VkCommandBuffer commandBuffer);

	inline VkDescriptorSetLayout getShadowMapLayout() {
		return shadowMapDescriptor.layout;
	}

	inline VkDescriptorSetLayout getLightMatrixLayout() {
		return lightDescriptor.layout;
	}

	inline std::vector<VkDescriptorSet> getShadowMap() {
		return shadowMapDescriptor.sets;
	}

	inline std::vector<VkDescriptorSet> getLightMatrix() {
		return lightDescriptor.sets;
	}

private:
    void updateLightMatrix(const Camera& camera, const std::vector<DirectionalLightBuffer> directionalLights, float aspect);
    VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	Descriptor lightDescriptor;
	Descriptor shadowMapDescriptor;

	ImageResource shadowMap;
	BufferResource shadowMapLight;

	CommonDescriptor& commonDescriptor;

	VkSampler sampler = VK_NULL_HANDLE;

	VkImageLayout currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
};
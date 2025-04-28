#pragma once

#include <vulkan/vulkan.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"

#include <memory>
#include <functional>
#include <vector>

#include "vulkan_types.hpp"

class GLFWwindow;

class VulkanGUI {
public:
    VulkanGUI() = default;
	~VulkanGUI() = default;

	void init(
		GLFWwindow* window,
		VkInstance instance,
		VkPhysicalDevice physicalDevice,
		VkDevice device,
		uint32_t graphicsFamilyIndex,
		VkQueue graphicsQueue,
		Swapchain& swapchain
	);
	void cleanup(VkDevice device);
	void createFramebuffer(VkDevice device, Swapchain& swapchain);
	void recreateFramebuffer(VkDevice device, Swapchain& swapchain);
	void render(VkCommandBuffer commandBuffer, VkExtent2D swapchainExtent, uint32_t imageIndex);

	int inline getMode() const {
		return mode;
	}
	float inline getIntensity() const {
		return intensity;
	}
	void inline proceedRenderModeIndex() {
		mode = (mode + 1) % std::size(renderModes);
	}
	void inline setRenderModeChangedCallback(std::function<void()> callback) {
		renderModeChangedCallback = callback;
	}
	void inline setRayTracingAvailable(bool rayTracingAvailable) {
		m_isRayTracingAvailable = rayTracingAvailable;
		renderModes = std::vector<const char*>(DEFALT_MODES.begin(), DEFALT_MODES.end());
		if (rayTracingAvailable) {
			renderModes.insert(renderModes.end(), RT_MODES.begin(), RT_MODES.end());
		}
	}
	bool inline isRayTracingAvailable() const {
		return m_isRayTracingAvailable;
	}
	bool inline isRayTracingMode() const {
		return mode >= DEFALT_MODES.size();
	}

private:
	void createDescriptorPool(VkDevice device);
	void createRenderPass(VkDevice device, VkFormat imageFormat);

	VkDescriptorPool descriptorPool;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer> framebuffers;

	const std::array<const char*, 3> DEFALT_MODES= {"Forward", "Deferred + ShadowMapping", "Pixel"};
	const std::array<const char*, 1> RT_MODES= {"(Real-Time) Ray Tracing in One Weekend"};
	std::vector<const char*> renderModes;

	std::function<void()> renderModeChangedCallback;

	// TODO separate state from GUI (adopt MV pattern)
	// manage states collectively for now
	int mode = 0;
	float intensity = 1.0f;
	bool m_isRayTracingAvailable = false;
};
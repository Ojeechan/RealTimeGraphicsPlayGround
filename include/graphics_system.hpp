#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <cstddef>

#include "vulkan_state.hpp"
#include "gui_renderpass.hpp"

class WindowState;
class PointLightBuffer;
class DirectionalLightBuffer;
class AssetData;
class Camera;
class DirectionalLightBuffer;

class GraphicsSystem {
public:
	GraphicsSystem(WindowState& windowState) : vulkanState(VulkanState(windowState)) {}
	~GraphicsSystem() = default;
	void init();
	void createLevelResource(size_t assetCount, size_t pointLightCount, size_t dirLightCount);
	ModelResource createModelResource(std::string textureDir, std::string modelDir, nlohmann::json data);
	void updateLights(std::vector<PointLightBuffer>& pointLights, std::vector<DirectionalLightBuffer>& directionalLights);
	void inline render(const std::vector<AssetData>& assets, const Camera& camera, const std::vector<DirectionalLightBuffer>& directionalLights) {
		vulkanState.updateCamera(camera);
		vulkanState.cleanupRenderModeResource();
		vulkanState.render(assets, camera, directionalLights);
	}
	void cleanup(AssetData& player, std::vector<AssetData>& props);
	void changeRenderPass() {
		vulkanState.changeRenderPass();
	}
private:
	VulkanState vulkanState;
};
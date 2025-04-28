#include "graphics_system.hpp"

#include <string>
#include <cstddef>
#include <vector>

#include "window_state.hpp"

void GraphicsSystem::init() {
	vulkanState.init();
}
void GraphicsSystem::createLevelResource(size_t assetCount, size_t pointLightCount, size_t dirLightCount) {
	vulkanState.createLevelResource(assetCount, pointLightCount, dirLightCount);
}
ModelResource GraphicsSystem::createModelResource(std::string textureDir, std::string modelDir, nlohmann::json data) {
	return vulkanState.createModelResource(textureDir, modelDir, data);
}
void GraphicsSystem::updateLights(std::vector<PointLightBuffer>& pointLights, std::vector<DirectionalLightBuffer>& directionalLights) {
	vulkanState.updateLightSSBO(pointLights, directionalLights);
}
void GraphicsSystem::cleanup(AssetData& player, std::vector<AssetData>& props) {
	vulkanState.deviceWaitIdle();
	vulkanState.cleanup(player, props);
}
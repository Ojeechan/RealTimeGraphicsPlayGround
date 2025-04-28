#include "rt_graphics_app.hpp"

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "window_state.hpp"
#include "input_manager.hpp"
#include "update_system.hpp"
#include "graphics_system.hpp"
#include "game_object.hpp"
#include "vulkan_types.hpp"
#include "buffer_types.hpp"

void RTGraphicsApp::run() {
	setCallback();
	graphicsSystem.init();
	loadAssets("../assets.json");
	auto lastTime = std::chrono::steady_clock::now();
	while (!windowState.windowShouldClose()) {
		glfwPollEvents();
		auto currentTime = std::chrono::steady_clock::now();
		delta = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
		lastTime = currentTime;
		updateSystem.update(player.value().object, camera, inputManager, delta);
		std::vector<AssetData> assets(props);
		assets.push_back(player.value());
		graphicsSystem.render(assets, camera, directionalLights);
	}
	graphicsSystem.cleanup(player.value(), props);
}

void RTGraphicsApp::setCallback() {
	GLFWwindow* window = windowState.getWindow();
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetKeyCallback(window, keyCallback);
}

void RTGraphicsApp::loadAssets(std::string filepath) {
	std::ifstream assetData(filepath);
	nlohmann::json json;
	assetData >> json;
	std::string textureDir = json["textureDir"];
	std::string modelDir = json["modelDir"];
	auto characterData = json["characters"];
	auto propsData = json["props"];
	auto pointLightData = json["lights"]["point"];
	auto directionalLightData = json["lights"]["directional"];

	graphicsSystem.createLevelResource(characterData.size() + propsData.size(), pointLightData.size(), directionalLightData.size());

	for (const auto& character : characterData) {
		// currently load the last character for the player
		glm::vec3 position(character["position"][0], character["position"][1], character["position"][2]);
		glm::vec3 direction(character["direction"][0], character["direction"][1], character["direction"][2]);
		GameObject gameObject(position, direction);
		AssetData characterAsset{gameObject};
		characterAsset.resource = graphicsSystem.createModelResource(textureDir, modelDir, character);
		player = characterAsset;
	}

	props.resize(propsData.size());
	for (size_t i = 0; i < propsData.size(); ++i) {
		glm::vec3 position(propsData[i]["position"][0], propsData[i]["position"][1], propsData[i]["position"][2]);
		glm::vec3 direction(propsData[i]["direction"][0], propsData[i]["direction"][1], propsData[i]["direction"][2]);
		GameObject gameObject(position, direction);
		AssetData propsAsset{gameObject};
		propsAsset.resource = graphicsSystem.createModelResource(textureDir, modelDir, propsData[i]);
		props[i] = propsAsset;
	}

	pointLights.resize(pointLightData.size());
	for (size_t i = 0; i < pointLightData.size(); ++i) {
		PointLightBuffer buffer;
		buffer.position = glm::vec3(
			static_cast<float>(pointLightData[i]["position"][0]),
			static_cast<float>(pointLightData[i]["position"][1]),
			static_cast<float>(pointLightData[i]["position"][2])
		);
		buffer.color = glm::vec3(
			static_cast<float>(pointLightData[i]["color"][0]),
			static_cast<float>(pointLightData[i]["color"][1]),
			static_cast<float>(pointLightData[i]["color"][2])
		);
		buffer.intensity = static_cast<float>(pointLightData[i]["intensity"]);
		pointLights[i] = buffer;
	}

	directionalLights.resize(directionalLightData.size());
	for (size_t i = 0; i < directionalLightData.size(); ++i) {
		DirectionalLightBuffer buffer;
		buffer.direction = glm::vec3(
			static_cast<float>(directionalLightData[i]["direction"][0]),
			static_cast<float>(directionalLightData[i]["direction"][1]),
			static_cast<float>(directionalLightData[i]["direction"][2])
		);
		buffer.color = glm::vec3(
			static_cast<float>(directionalLightData[i]["color"][0]),
			static_cast<float>(directionalLightData[i]["color"][1]),
			static_cast<float>(directionalLightData[i]["color"][2])
		);
		buffer.intensity = static_cast<float>(directionalLightData[i]["intensity"]);
		directionalLights[i] = buffer;
	}

	// TODO Enable real-time modification of light parameters
	graphicsSystem.updateLights(pointLights, directionalLights);

}

inline void RTGraphicsApp::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
	auto app = static_cast<RTGraphicsApp*>(glfwGetWindowUserPointer(window));
}
void RTGraphicsApp::cursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
	auto app = static_cast<RTGraphicsApp*>(glfwGetWindowUserPointer(window));
}

void RTGraphicsApp::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	auto app = static_cast<RTGraphicsApp*>(glfwGetWindowUserPointer(window));

	if (action == GLFW_RELEASE) {
		app->inputManager.keyReleased(key);
		return;
	}

	// toggle
	switch (key) {
		case GLFW_KEY_R:
			if (app->inputManager.isKeyPressed(GLFW_KEY_R)) {
				break;
			}
			app->graphicsSystem.changeRenderPass();
			break;
		case GLFW_KEY_P:
			if (app->inputManager.isKeyPressed(GLFW_KEY_P)) {
				break;
			}
			app->camera.togglePerspective();
			break;
	}

	if (action == GLFW_PRESS) {
		app->inputManager.keyPressed(key);
	}
}

void RTGraphicsApp::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<RTGraphicsApp*>(glfwGetWindowUserPointer(window));
	app->windowState.setFramebufferResized(true);
}
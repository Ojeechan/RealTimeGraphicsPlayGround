#pragma once

#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>
#include <vector>
#include <optional>

#include "window_state.hpp"
#include "input_manager.hpp"
#include "update_system.hpp"
#include "graphics_system.hpp"
#include "camera.hpp"

class RTGraphicsApp {
public:
	RTGraphicsApp() : windowState(800, 600, "Real-Time Graphics Playground"), graphicsSystem(windowState) {}
	~RTGraphicsApp() = default;
	void run();
private:
	void setCallback();
	void loadAssets(std::string filepath);
	static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
	static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos);
	static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

	WindowState windowState;
	InputManager inputManager;
	UpdateSystem updateSystem;
	GraphicsSystem graphicsSystem;

	// TODO move to state management class
	std::optional<AssetData> player;
	std::vector<AssetData> props;
	Camera camera;
	std::vector<PointLightBuffer> pointLights;
	std::vector<DirectionalLightBuffer> directionalLights;

	float delta = 0.0f;
};
#include "window_state.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

WindowState::WindowState(int width, int height, const char* title) {
	if (!glfwInit()) {
		throw std::runtime_error("failed to init glfw");
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		throw std::runtime_error("failed to create window");
	}
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GLFW_TRUE);
}

WindowState::~WindowState() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

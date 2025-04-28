#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class WindowState {
public:
	WindowState() = delete;
	WindowState(int width, int height, const char* title);
	~WindowState();
	inline bool windowShouldClose() const {
		return glfwWindowShouldClose(window);
	}
	inline GLFWwindow* getWindow() const {
		return window;
	}
	inline bool isFramebufferResized() const {
		return framebufferResized;
	}
	inline void setFramebufferResized(bool framebufferResized) {
		this->framebufferResized = framebufferResized;
	}
private:
	GLFWwindow* window;
	bool framebufferResized = false;
};
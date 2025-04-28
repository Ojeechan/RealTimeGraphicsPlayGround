#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class InputManager {
public:
	InputManager() = default;
	~InputManager() = default;
	inline bool isKeyPressed(int key) const {
		if (key < 0 || key >= GLFW_KEY_LAST) {
			return false;
		}

		return keys[key];
	}
	inline void keyPressed(int key) {
		keys[key] = true;
	}
	inline void keyReleased(int key) {
		keys[key] = false;
	}

private:
	bool keys[GLFW_KEY_LAST] = { false };
};
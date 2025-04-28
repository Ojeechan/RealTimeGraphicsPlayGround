#include "update_system.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "input_manager.hpp"
#include "game_object.hpp"
#include "camera.hpp"

void UpdateSystem::update(GameObject& player, Camera& camera, InputManager& inputManager, float delta) {
	if (inputManager.isKeyPressed(GLFW_KEY_A)) {
		player.move(LEFT, delta);
	}
	if (inputManager.isKeyPressed(GLFW_KEY_D)) {
		player.move(RIGHT, delta);
	}
	if (inputManager.isKeyPressed(GLFW_KEY_W)) {
		player.move(FORWARD, delta);
	}
	if (inputManager.isKeyPressed(GLFW_KEY_S)) {
		player.move(BACKWARD, delta);
	}
	if (inputManager.isKeyPressed(GLFW_KEY_SPACE)) {
		player.jump(delta);
		inputManager.keyReleased(GLFW_KEY_SPACE);
	}
	player.fall(delta);
	player.update();

	if (inputManager.isKeyPressed(GLFW_KEY_LEFT)) {
		if(inputManager.isKeyPressed(GLFW_KEY_LEFT_SHIFT) || inputManager.isKeyPressed(GLFW_KEY_RIGHT_SHIFT)) {
			camera.moveLeft(delta);
		} else {
			camera.panLeft(delta);
		}
	}
	if (inputManager.isKeyPressed(GLFW_KEY_RIGHT)) {
		if(inputManager.isKeyPressed(GLFW_KEY_LEFT_SHIFT) || inputManager.isKeyPressed(GLFW_KEY_RIGHT_SHIFT)) {
			camera.moveRight(delta);
		} else {
			camera.panRight(delta);
		}
	}
	if (inputManager.isKeyPressed(GLFW_KEY_UP)) {
		camera.moveForward(delta);
	}
	if (inputManager.isKeyPressed(GLFW_KEY_DOWN)) {
		camera.moveBackward(delta);
	}
}
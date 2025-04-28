#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

class GameObject;
class Camera;
class InputManager;

class UpdateSystem {
public:
	UpdateSystem() = default;
	~UpdateSystem() = default;
	static constexpr glm::vec3 FORWARD = glm::vec3(0.0f, 0.0f, -1.0f);
	static constexpr glm::vec3 BACKWARD = glm::vec3(0.0f, 0.0f, 1.0f);
	static constexpr glm::vec3 LEFT = glm::vec3(-1.0f, 0.0f, 0.0f);
	static constexpr glm::vec3 RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
	static constexpr glm::vec3 UP = glm::vec3(0.0f, 1.0f, 0.0f);
	static constexpr glm::vec3 DOWN = glm::vec3(0.0f, -1.0f, 0.0f);

	void update(GameObject& player, Camera& camera, InputManager& inputManager, float delta);
};
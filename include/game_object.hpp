#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <algorithm>

class GameObject {
public:
	GameObject() = default;
	GameObject(glm::vec3 position, glm::vec3 direction) : m_position(position), m_direction(direction) {};
	~GameObject() = default;

	static constexpr float VELOCITY = 10.0f;
	static constexpr float TURN_SPEED = 5.0f;
	static constexpr float JUMP = 50.0f;
	static constexpr float GRAVITY = -9.8f;

	inline void move(glm::vec3 direction, float delta) {
		m_velocity += direction * VELOCITY * delta;
	}
	inline void jump(float delta) {
		m_velocity.y += JUMP * delta;
	}
	inline void fall(float delta) {
		m_velocity.y += GRAVITY * delta;
	}
	inline void update() {
		m_position += m_velocity;

		if (m_velocity.x != 0.0f || m_velocity.z != 0.0f) {
			m_direction = m_velocity;
		}

		m_velocity.x = 0.0f;
		m_velocity.z = 0.0f;

		m_position.y = std::max<float>(m_position.y, 0.0f);
		if (m_position.y <= 0.0f) {
			m_velocity.y = 0.0f;
		}
	}
	inline glm::vec3 getPosition() const {
		return m_position;
	}
	inline glm::vec3 getDirection() const {
		return m_direction;
	}
	inline glm::mat4 getModelMatrix() const {
		glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), m_position);
		float yaw = glm::atan(-m_direction.z, m_direction.x);
		modelMatrix = modelMatrix * glm::rotate(modelMatrix, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		return modelMatrix;
	}
private:
	glm::vec3 m_position;
	glm::vec3 m_direction = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 m_velocity;
};
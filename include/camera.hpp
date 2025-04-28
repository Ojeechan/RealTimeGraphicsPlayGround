#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

class Camera {
public:
  Camera() = default;
  ~Camera() = default;
  static constexpr float VELOCITY = 30.0f;
  static constexpr float PAN_SPEED = 60.0f;
	inline void moveLeft(float delta) {
    glm::vec3 right = glm::normalize(glm::cross(front, up));
		glm::vec3 velocity =  -VELOCITY * delta * right;
    position += velocity;
	};
	inline void moveRight(float delta) {
    glm::vec3 right = glm::normalize(glm::cross(front, up));
		glm::vec3 velocity = VELOCITY * delta * right;
    position += velocity;
	};
	inline void moveForward(float delta) {
		glm::vec3 velocity =  VELOCITY * delta * front;
    position += velocity;
	};
	inline void moveBackward(float delta) {
		glm::vec3 velocity = -VELOCITY * delta * front;
    position += velocity;
	};
  inline void panLeft(float delta) {
		glm::mat4 rotation = glm::rotate(glm::mat4(1.0), glm::radians(PAN_SPEED * delta), up);
	  front = glm::mat3(rotation) * front;
  };
  inline void panRight(float delta) {
		glm::mat4 rotation = glm::rotate(glm::mat4(1.0), glm::radians(-PAN_SPEED * delta), up);
	  front = glm::mat3(rotation) * front;
  };
  inline glm::mat4 getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
  };
  inline glm::vec3 getPosition() const {
    return position;
  }
  inline glm::vec3 getFront() const {
    return front;
  }
  inline glm::vec3 getUp() const {
    return up;
  }
  inline float getFOV() const {
    return fov;
  }
  inline float getFarPlane() const {
    return farPlane;
  }
  inline float getNearPlane() const {
    return nearPlane;
  }
  inline bool isPerspective() const {
    return perspective;
  }
  inline void togglePerspective() {
    perspective = !perspective;
  }
private:
  glm::vec3 position = glm::vec3(0.0f, 2.0f, 10.0f);
  glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

  float nearPlane = 0.1f;
  float farPlane = 100.0f;
  float fov = glm::radians(45.0f);

  bool perspective = true;
};
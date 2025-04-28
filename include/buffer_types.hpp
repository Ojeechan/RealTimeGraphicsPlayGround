#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

struct TransformMatrixBuffer {
	alignas(16) glm::mat4 model;
};

struct CameraMatrixBuffer {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

struct CameraBuffer {
	alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 front;
    alignas(16) glm::vec3 up;
    float fov;
};

struct PointLightBuffer {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 color;
    float intensity;
};

struct DirectionalLightBuffer {
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;
    float intensity;
};
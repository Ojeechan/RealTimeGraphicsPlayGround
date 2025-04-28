#version 460

layout(set= 0, binding = 0) uniform CameraMatrix {
    mat4 view;
    mat4 proj;
} cameraMat;

layout(set= 1, binding = 0) uniform Camera {
    vec3 position;
    vec3 front;
    vec3 up;
    float fov;
} camera;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

layout(std430, set = 2, binding = 0) buffer PointLightBuffer {
    PointLight[] pointLights;
};

layout(std430, set = 2, binding = 1) buffer DirectionalLightBuffer {
    DirectionalLight[] directionalLights;
};
layout(set = 3, binding = 0) uniform sampler2D gAlbedo;
layout(set = 3, binding = 1) uniform sampler2D gPosition;
layout(set = 3, binding = 2) uniform sampler2D gNormal;
layout(set = 3, binding = 3) uniform sampler2D gMaterial;
layout(set = 3, binding = 4) uniform sampler2D gDepth;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

int BLOCK_SIZE = 8;
layout(push_constant) uniform PushConstant {
    vec2 resolution;
};

void main() {
    vec2 pixelCoord = inTexCoord * resolution;
	pixelCoord = clamp(pixelCoord, vec2(0.0), resolution - BLOCK_SIZE);
    vec2 blockCoord = floor(pixelCoord / BLOCK_SIZE) * BLOCK_SIZE;
	blockCoord = blockCoord + BLOCK_SIZE * 0.5;
    vec2 mosaicUV = blockCoord / resolution;
    outColor = texture(gAlbedo, mosaicUV);
}
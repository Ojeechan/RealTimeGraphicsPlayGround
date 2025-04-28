#version 460

layout(set= 0, binding = 0) uniform ModelMatrix {
    mat4 model;
} modelMat;

layout(set= 1, binding = 0) uniform CameraMatrix {
    mat4 view;
    mat4 proj;
} cameraMat;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outPosition;

void main() {
    gl_Position = cameraMat.proj * cameraMat.view * modelMat.model * vec4(inPosition, 1.0);
    outNormal = normalize(mat3(modelMat.model) * inNormal);
    outColor = inColor;
    outTexCoord = inTexCoord;
    outPosition = vec3(modelMat.model * vec4(inPosition, 1.0));
}
#version 460

layout(set= 0, binding = 0) uniform CameraMatrix {
    mat4 view;
    mat4 proj;
} cameraMat;

layout(set= 1, binding = 0) uniform Camera {
    vec3 position;
    vec3 front;
    vec3 up;
} cam;

layout(input_attachment_index = 2, set = 2, binding = 2) uniform subpassInput gNormal;
layout(input_attachment_index = 4, set = 2, binding = 4) uniform subpassInput gDepth;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
};

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out float ssaoOutput;

float rand(vec2 v) {
    // familiar shader rand
    return fract(sin(dot(v.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    // TODO implement SSAO
}
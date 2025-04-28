#version 460

layout(set = 2, binding = 0) uniform Camera {
    vec3 position;
    vec3 front;
    vec3 up;
    float fov;
} camera;
layout(set = 3, binding = 0) uniform sampler2D albedoSampler;
layout(set = 3, binding = 1) uniform sampler2D normalSampler;
layout(set = 3, binding = 2) uniform sampler2D materialSampler;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inPosition;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outPosition;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec2 outMaterial;

float NEAR = 0.1;
float FAR = 100.0;

void main() {
	outAlbedo = texture(albedoSampler, inTexCoord);
	outPosition = vec4(inPosition, 1.0);
	outNormal = vec4(normalize(inNormal), 1.0);
	outMaterial = vec2(texture(materialSampler, inTexCoord).rg);

    float z = gl_FragCoord.z;

	// TODO parameterize near, far plane
    float linearDepth = (NEAR * FAR) / (FAR - z * (FAR - NEAR)) / FAR;
	gl_FragDepth = linearDepth;
}
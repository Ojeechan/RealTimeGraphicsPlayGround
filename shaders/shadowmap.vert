#version 460

layout(set = 0, binding = 0) uniform ModelUBO {
    mat4 model;
} model;

layout(set = 1, binding = 0) uniform LightUBO {
    mat4 view;
	mat4 proj;

} light;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = light.proj * light.view * model.model * vec4(inPosition, 1.0);
}
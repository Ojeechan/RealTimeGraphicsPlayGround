#version 460

layout(set = 0, binding = 0) uniform sampler2D renderedImage;

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(texture(renderedImage, inTexCoord).rgb, 1.0);
}
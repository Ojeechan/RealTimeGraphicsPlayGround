#version 460

vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0)
);

layout(location = 0) out vec2 outTexCoord;

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    outTexCoord = gl_Position.xy * 0.5 + 0.5;
}
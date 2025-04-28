#version 460
#extension GL_EXT_ray_tracing : require


struct Payload {
    vec2 nCoord;
    vec3 color;
    int depth;
};

layout(location = 0) rayPayloadInEXT Payload payload;

void main()
{
    float a = (payload.nCoord.y + 1.0) * 0.5;
    payload.color = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), a);
}
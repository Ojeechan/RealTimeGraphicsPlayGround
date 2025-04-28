#version 460
#extension GL_EXT_ray_tracing : require

// structs
struct Sphere {
    vec3 center;
    vec3 color;
    float radius;
};

struct Payload {
    vec2 nCoord;
    vec3 color;
    int depth;
};

struct HitAttribute {
    vec3 position;
    vec3 normal;
};

// buffers
layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(std430, set = 4, binding = 0) buffer SphereBuffer {
    Sphere[] spheres;
};

// push constants
layout(push_constant) uniform PushConstants {
    vec2  windowSize;
    float seed;
} pc;

// hit attribute from intersect shader
hitAttributeEXT HitAttribute hitAttr;

// payload
layout(location = 0) rayPayloadInEXT Payload payload;

// consts
float BIAS = 0.001;
float MAX_DEPTH = 31;
float REFRACTION_INDEX = 1.5;

// random utils
float rand(float x) {
    return fract(sin(x) * 55555.7777777);
}

float randRange(float x) {
    return 2.0 * rand(x) - 1.0;
}

bool nearZero(vec3 v) {
    float s = 1e-8;
    return (abs(v.x) < s) && (abs(v.y) < s) && (abs(v.z) < s);
}

vec3 randVec3(vec3 v) {
    vec3 randVec = vec3(randRange(v.x), randRange(v.y), randRange(v.z));
    if(nearZero(randVec)) {
        return hitAttr.normal;
    }
    return normalize(randVec);
}

vec3 randOnHemisphere(vec3 position) {
    vec3 onUnitSphere = normalize(randVec3(position));
    if (dot(onUnitSphere, hitAttr.normal) > 0.0) {
        return onUnitSphere;
    }
    return -onUnitSphere;
}

bool isFrontFace() {
    return dot(normalize(gl_WorldRayDirectionEXT), hitAttr.normal) > 0.0 ? false : true;
}

 float reflectance(float cosine, float ri) {
    float r0 = (1.0 - ri) / (1.0 + ri);
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5);
}

void main() {
    if (payload.depth++ > MAX_DEPTH) {
        payload.color = vec3(0.0);
        return;
    }
    Sphere s = spheres[gl_InstanceCustomIndexEXT];

    float ri = isFrontFace() ? (1.0 / REFRACTION_INDEX) : REFRACTION_INDEX;
    float cosTheta = min(dot(-gl_WorldRayDirectionEXT, hitAttr.normal), 1.0);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    bool cannotRefract = ri * sinTheta > 1.0;

    vec3 secondaryDirection;
    // if (cannotRefract || reflectance(cosTheta, ri) > rand(hitAttr.position.x)) { // infeasible for real time
    if (cannotRefract) {
        secondaryDirection = reflect(gl_WorldRayDirectionEXT, hitAttr.normal);
    } else {
        secondaryDirection = refract(gl_WorldRayDirectionEXT, hitAttr.normal, ri);
    }
    vec3 secondaryOrigin = hitAttr.position + BIAS * hitAttr.normal;

    traceRayEXT(
       topLevelAS,
       gl_RayFlagsOpaqueEXT,
       0xFF,
       0,
       0,
       0,
       secondaryOrigin,
       BIAS,
       secondaryDirection,
       10000.0,
       0
    );
    payload.color *= 1.0; // no attenuation
}
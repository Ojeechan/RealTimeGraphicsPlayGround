#version 460

layout(set = 2, binding = 0) uniform Camera {
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

layout(std430, set = 3, binding = 0) buffer PointLightBuffer {
    PointLight[] pointLights;
};

layout(std430, set = 3, binding = 1) buffer DirectionalLightBuffer {
    DirectionalLight[] directionalLights;
};

layout(set = 4, binding = 0) uniform sampler2D albedoSampler;
layout(set = 4, binding = 1) uniform sampler2D normalSampler;
layout(set = 4, binding = 2) uniform sampler2D materialSampler;

layout(set = 5, binding = 0, rgba32f) uniform writeonly image2D outputImage;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inPosition;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// RT-Rendering (9.41): GGX distribution
float D(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// RT-Rendering (9.44): Karis G1
float G1(float NdotV, float roughness) {
    float nominator = 2 * NdotV;
    float denominator = NdotV * (2 - roughness) + roughness;
    return nominator / denominator;
}

// lambda function for GGX NDF
float lambdaGGX(float NdotV, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotV2 = NdotV * NdotV;
    return (sqrt(alpha2 + (1.0 - alpha2) * NdotV2) - NdotV) / NdotV;
}

// RT-Rendering (9.24): Smith G1 + GGX
// float G1(float NdotV, float roughness) {
//     float lambda = lambdaGGX(NdotV, roughness);
//     return max(NdotV, 0.0) / (1.0 + lambda);
// }

// RT-Rendering (9.27): use the simplest approximation
float G2(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = G1(NdotV, roughness);
    float ggx2 = G1(NdotL, roughness);

    return ggx1 * ggx2;
}

// RT-Rendering (9.16)
vec3 schlickFresnelApprox(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    vec3 albedo = texture(albedoSampler, inTexCoord).rgb;
    vec2 material = texture(materialSampler, inTexCoord).rg;
    float metallic = material.r;
    float roughness = material.g;

    DirectionalLight dLight = directionalLights[0];

    vec3 N = normalize(inNormal);
    vec3 V = normalize(camera.position - inPosition);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 color = vec3(0.0);
    for (int i = 0; i < 1; i++) {
        DirectionalLight dLight = directionalLights[i];
        vec3 L = normalize(-dLight.direction);
        vec3 H = normalize(V + L);

        float NDF = D(N, H, roughness);
        vec3 F = schlickFresnelApprox(max(dot(H, V), 0.0), F0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float G = G2(N, V, L, roughness);
        float NdotL = max(dot(N, L), 0.0);
        vec3 nominator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;

        // Cook-Torrance specular model
        vec3 specular = nominator / denominator;

        // simple Lambertian diffuse model
        // TODO use Oren-Nayer diffuse model
        vec3 diffuse = kD * albedo / PI;

        vec3 lightColor = dLight.color * dLight.intensity;
        color += (diffuse + specular) * lightColor * NdotL;
    }

    outColor = vec4(color, 1.0);
}
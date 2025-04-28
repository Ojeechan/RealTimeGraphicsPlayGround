#version 460

layout(set= 0, binding = 0) uniform CameraMatrix {
    mat4 view;
    mat4 proj;
} cameraMat;

layout(set = 1, binding = 0) uniform Camera {
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
layout(input_attachment_index = 0, set = 3, binding = 0) uniform subpassInput gAlbedo;
layout(input_attachment_index = 1, set = 3, binding = 1) uniform subpassInput gPosition;
layout(input_attachment_index = 2, set = 3, binding = 2) uniform subpassInput gNormal;
layout(input_attachment_index = 3, set = 3, binding = 3) uniform subpassInput gMaterial;
layout(input_attachment_index = 4, set = 3, binding = 4) uniform subpassInput gDepth;
layout(input_attachment_index = 5, set = 3, binding = 5) uniform subpassInput gSSAO;

layout(set = 4, binding = 0) uniform sampler2D shadowMap;

layout(set= 5, binding = 0) uniform LightMatrix {
    mat4 view;
    mat4 proj;
} lightMat;

layout(location = 0) in vec2 inTexCoord;
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

float calculateShadow(vec3 worldPosition) {
    vec4 lightSpacePosition = lightMat.proj * lightMat.view * vec4(worldPosition, 1.0);
    vec3 lightSpaceCoords = lightSpacePosition.xyz / lightSpacePosition.w;
    lightSpaceCoords = lightSpaceCoords * 0.5 + 0.5;

    float shadowMapDepth = texture(shadowMap, lightSpaceCoords.xy).r;
    float currentDepth = lightSpaceCoords.z - 0.0005;

    return (shadowMapDepth < currentDepth) ? 0.4 : 1.0;
}

void main() {
    float depth = subpassLoad(gDepth).r;
    if (depth == 1.0) {
        outColor = vec4(0.5, 0.8, 1.0, 0.7);
        return;
    }

    vec3 albedo = subpassLoad(gAlbedo).rgb;
    vec3 position = subpassLoad(gPosition).rgb;
    vec2 material = subpassLoad(gMaterial).rg;
    float metallic = material.r;
    float roughness = material.g;

    DirectionalLight dLight = directionalLights[0];

    vec3 N = normalize(subpassLoad(gNormal).rgb);
    vec3 V = normalize(camera.position - position);
    vec3 L = normalize(-dLight.direction);
    vec3 H = normalize(V + L);

    float NDF = D(N, H, roughness);
    float G = G2(N, V, L, roughness);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = schlickFresnelApprox(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 nominator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;

    // Cook-Torrance specular model
    vec3 specular = nominator / denominator;

    // simple Lambertian diffuse model
    // TODO use Oren-Nayer diffuse model
    vec3 diffuse = kD * albedo / PI;

    vec3 lightColor = dLight.color * dLight.intensity;
    vec3 baseColor = (diffuse + specular) * lightColor * NdotL;

    float shadow = calculateShadow(position);

    vec3 finalColor = baseColor * shadow;

    outColor = vec4(finalColor, 1.0);
}
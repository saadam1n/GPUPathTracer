#ifndef MICROFACET_GLSL
#define MICROFACET_GLSL

#include "Random.glsl"
#include "Material.glsl"
#include "Constants.glsl"

// Microfacet distributions, importance sampling strategies, and PDFs

// Trowbridge-Reitz (GGX)
// Microfacet distribution
float MicrofacetDistributionTrowbridgeReitz(in MaterialInstance material, in SurfaceInteraction interaction) {
    float divsor = (material.roughness2 - 1.0f) * interaction.ndm2 + 1.0f;
    return material.roughness2 / max(M_PI * divsor * divsor, 1e-10f);
}

// Importance sample
vec3 ImportanceSampleTrowbridgeReitz(in MaterialInstance material) {
    vec2 r = rand2();
    float nch = sqrt((1.0f - r.x) / (r.x * (material.roughness2 - 1.0f) + 1.0f));
    float nsh = sqrt(1.0f - nch * nch);

    r.y *= 2.0f * M_PI;

    vec3 direction;
    direction.x = nsh * sin(r.y);
    direction.y = nsh * cos(r.y);
    direction.z = nch;

    return direction;
}

// Probability density
float ProbabilityDensityTrowbridgeReitz(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return max(MicrofacetDistributionTrowbridgeReitz(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-32f);
}

// Beckmann (Gaussian)
// Microfacet distribution
float MicrofacetDistributionBeckmann(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    float sub = 2.0f * log(sqrt(M_PI) * material.roughness * interaction.ndm);
    float add = (interaction.ndm2 - 1.0f) / (interaction.ndm2 * material.roughness2);
    return exp(add - sub);
}

// Importance sample - "Microfacet Models for Refraction through Rough Surfaces", eqs 28 and 29"
vec3 ImportanceSampleBeckmann(inout MaterialInstance material) {
    vec2 r = rand2();
    float g = -material.roughness2 * log(1 - r.x);
    float z2 = 1.0f / (1.0f + g);
    float z = sqrt(z2);
    float phi = 2 * M_PI * r.y;
    float radius = sqrt(1.0 - z2);
    vec3 direction = vec3(radius * vec2(sin(phi), cos(phi)), z);
    return direction;
}

// Probability density
float ProbabilityDensityBeckmann(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return max(MicrofacetDistributionBeckmann(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-32f);
}

// Blinn-Phong (cosine-power)
// Conversion from Beckmann
float ConvertBeckmannToBlinnPhong(inout MaterialInstance material) {
    return 2.0f / material.roughness - 2.0f;
}

// Microfacet distribution
// See "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling" for a detailed view on the equations
float MicrofacetDistributionBlinnPhong(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    float n = ConvertBeckmannToBlinnPhong(material);
    float normalization = (n + 1.0f) / (2.0f * M_PI);
    return normalization * pow(interaction.ndm, n);
}

// Importance sample
vec3 ImportanceSampleBlinnPhong(inout MaterialInstance material) {
    vec2 r = rand2();
    float n = ConvertBeckmannToBlinnPhong(material);
    float z = pow(r.x, 1.0f / (n + 1.0f));
    float phi = 2 * M_PI * r.y;
    float radius = sqrt(1.0f - z * z);
    vec3 direction = vec3(radius * vec2(sin(phi), cos(phi)), z);
    return direction;
}

// Probability density
float ProbabilityDensityBlinnPhong(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return max(MicrofacetDistributionBlinnPhong(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-32f);
}

// Macro defines to choose which microfacet model to use
// For obj (which only support blinn-Phong), Beckmann should be used instead due to its similarily with a cosine-power distirubiton
// In all other cases, use Trowbridge-Reitz (GGX)!
#define MicrofacetDistribution(m, i) MicrofacetDistributionTrowbridgeReitz(m, i)
#define ImportanceSampleMicrofacet(m, i) ImportanceSampleTrowbridgeReitz(m)
#define ProbabilityDensityMicrofacet(m, i) ProbabilityDensityTrowbridgeReitz(m, i)

// Fernsel functions. The model used depends on the parameters of the material

vec3 FresnelSchlick(inout vec3 f0, inout float ndo) {
    float x = 1.0f - ndo;
    return f0 + (1.0f - f0) * (x * x * x * x * x);
}

#define Fresnel(m, d) FresnelSchlick(m.reflectance, d) // d is the dot product of the angle

// Geometry term. There's really just one, and that's the smith/schlick model

float GeometricShadowingSchlick(inout float k, inout float ndo) {
    return ndo / (ndo * (1.0f - k) + k);
}

float GeometricShadowingSmith(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    float k = material.roughness + 1.0;
    k *= k / 8.0f;
    return GeometricShadowingSchlick(k, interaction.ndi) * GeometricShadowingSchlick(k, interaction.ndo);
}

#define GeometricShadowing(m, i) GeometricShadowingSmith(m, i)

// Misc

float ProbabilityDensityCosine(inout SurfaceInteraction interaction) {
    return interaction.ndi / M_PI;
}

vec3 ImportanceSampleCosine() {
    vec2 r = rand2();
    float radius = sqrt(r.x);
    float phi = 2 * M_PI * r.y;
    float z = sqrt(1.0 - r.x);
    return vec3(radius * vec2(sin(phi), cos(phi)), z);
}

float ProbabilityDensityDirection(inout MaterialInstance material, in SurfaceInteraction interaction) {
    float pdf0 = ProbabilityDensityCosine(interaction);
    float pdf1 = ProbabilityDensityMicrofacet(material, interaction);
    return 0.5f * (pdf0 + pdf1);
}

vec3 GenerateImportanceSample(inout MaterialInstance material, inout SurfaceInteraction interaction, out float pdf) {
    if (rand() > 0.5f) {
        interaction.microfacet = interaction.tbn * ImportanceSampleMicrofacet(material, interaction);
        SetIncomingDirection(interaction, reflect(-interaction.outgoing, interaction.microfacet));
    }
    else {
        SetIncomingDirection(interaction, interaction.tbn * ImportanceSampleCosine());
    }
    pdf = ProbabilityDensityDirection(material, interaction);
    return interaction.incoming;
}

#endif
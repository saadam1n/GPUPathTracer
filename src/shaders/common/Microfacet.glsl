#ifndef MICROFACET_GLSL
#define MICROFACET_GLSL

#include "Random.glsl"
#include "Material.glsl"
#include "Constants.glsl"
#include "MIS.glsl"

// Microfacet distributions, importance sampling strategies, and PDFs

// Trowbridge-Reitz (GGX)
// Microfacet distribution
float MicrofacetDistributionTrowbridgeReitz(in MaterialInstance material, in SurfaceInteraction interaction) {
    float divsor = (material.roughness2 - 1.0f) * interaction.ndm2 + 1.0f;
    return material.roughness2 / max(M_PI * divsor * divsor, 1e-20f);
}

// Importance sample
vec3 ImportanceSampleTrowbridgeReitz(in MaterialInstance material) {
    vec2 r = rand2();
    float z2 = max((1.0f - r.x) / (r.x * (material.roughness2 - 1.0f) + 1.0f), 0.0f);
    float z = sqrt(z2);
    float phi = 2.0f * M_PI * r.y;
    float radius = sqrt(max(1.0f - z2, 0.0f));
    return vec3(radius * vec2(sin(phi), cos(phi)), z);
}

// Probability density
float ProbabilityDensityTrowbridgeReitz(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return max(MicrofacetDistributionTrowbridgeReitz(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-20f);
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
    float phi = 2.0f * M_PI * r.y;
    float radius = sqrt(1.0 - z2);
    return vec3(radius * vec2(sin(phi), cos(phi)), z);
}

// Probability density
float ProbabilityDensityBeckmann(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return max(MicrofacetDistributionBeckmann(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-10f);
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
    return max(MicrofacetDistributionBlinnPhong(material, interaction) * interaction.ndm / (4.0f * interaction.idm), 1e-10f);
}

// Macro defines to choose which microfacet model to use
// For obj (which only support blinn-Phong), Beckmann should be used instead due to its similarily with a cosine-power distirubiton
// In all other cases, use Trowbridge-Reitz (GGX)!
#define MicrofacetDistribution(m, i) MicrofacetDistributionTrowbridgeReitz(m, i)
#define ImportanceSampleMicrofacet(m) ImportanceSampleTrowbridgeReitz(m)
#define ProbabilityDensityMicrofacet(m, i) ProbabilityDensityTrowbridgeReitz(m, i)

// Fernsel functions. The model used depends on the parameters of the material

vec3 FresnelSchlick(inout vec3 f0, inout float ndo) {
    float x = 1.0f - ndo;
    return f0 + (1.0f - f0) * (x * x * x * x * x);
}

#define Fresnel(m, d) FresnelSchlick(m.reflectance, d) // d is the dot product of the angle

// Geometry term. There's really just one, and that's the smith/schlick model

// Taken from Filament's PBR
float GeometricShadowingSchlick(inout MaterialInstance material, inout float ndo) {
    float numer = 2.0f * ndo;
    float denom = ndo + sqrt(material.roughness2 + (1.0f - material.roughness2) * ndo * ndo);
    return numer / denom;
}

float GeometricShadowingSmith(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return GeometricShadowingSchlick(material, interaction.ndi) * GeometricShadowingSchlick(material, interaction.ndo);
}

float VisibilityGGX(inout MaterialInstance material, inout float ndo) {
    return 1.0f / (ndo + sqrt(material.roughness2 * (1.0f - material.roughness2) * ndo * ndo));
}

float VisibilitySmith(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return VisibilityGGX(material, interaction.ndi) * VisibilityGGX(material, interaction.ndo) / 4.0f;
}

float VisibilitySmithCorrelated(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    float so = interaction.ndi * sqrt(interaction.ndo * interaction.ndo * (1.0 - material.roughness2) + material.roughness2);
    float si = interaction.ndo * sqrt(interaction.ndi * interaction.ndi * (1.0 - material.roughness2) + material.roughness2);
    return 0.5f / (so + si);
}

float VisibilityImplicit(inout MaterialInstance material, inout SurfaceInteraction interaction) {
    return 1.0f / 4.0f;
}

#define GeometricShadowing(m, i) GeometricShadowingSmith(m, i)
#define Visibility(m, i) VisibilitySmith(m, i)

// Misc

vec3 DiffuseEnergyConservation(inout MaterialInstance material, in SurfaceInteraction interaction) {
    return (1.0f - material.metallic) * (1.0f - Fresnel(material, interaction.ndi)) * (1.0f - Fresnel(material, interaction.ndo));
}

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

// TODO: update this so NEE doesn't break
float ProbabilityDensityDirection(inout MaterialInstance material, in SurfaceInteraction interaction, float diffusePmf) {
    return diffusePmf * ProbabilityDensityCosine(interaction) + (1.0f - diffusePmf) * ProbabilityDensityMicrofacet(material, interaction);
}

float ProbabilityDensityDirection(inout MaterialInstance material, in SurfaceInteraction interaction) {
    interaction.ndi = 1.0f;
    float diffusePmf = AverageLuminance(DiffuseEnergyConservation(material, interaction));

    float pdfDiffuse = diffusePmf * ProbabilityDensityCosine(interaction);
    float pdfSpecular = (1.0f - diffusePmf) * ProbabilityDensityMicrofacet(material, interaction);
    float pdf = pdfDiffuse / MISWeight(pdfDiffuse, pdfSpecular);
    return pdf;
}

vec3 GenerateImportanceSample(inout MaterialInstance material, inout SurfaceInteraction interaction, out float pdf) {
    float diffusePmf = clamp(0.5, 0.0f, 0.75f);
    // Choose between specular and diffuse based on PDF
    if (rand() < diffusePmf) {
        // Use diffuse PDF
        SetIncomingDirection(interaction, interaction.tbn * ImportanceSampleCosine());
        float pdfDiffuse = diffusePmf * ProbabilityDensityCosine(interaction);
        float pdfSpecular = (1.0f - diffusePmf) * ProbabilityDensityMicrofacet(material, interaction);
        pdf = pdfDiffuse / MISWeight(pdfDiffuse, pdfSpecular);
    }
    else {
        // Use specular PDF
        SetMicrofacetDirection(interaction, interaction.tbn * ImportanceSampleMicrofacet(material));
        float pdfDiffuse = diffusePmf * ProbabilityDensityCosine(interaction);
        float pdfSpecular = (1.0f - diffusePmf) * ProbabilityDensityMicrofacet(material, interaction);
        pdf = pdfSpecular / MISWeight(pdfSpecular, pdfDiffuse);
    }
    return interaction.incoming;
}

#endif
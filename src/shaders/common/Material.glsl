#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "Random.glsl"
#include "Constants.glsl"
#include "Util.glsl"

// TODO: REWRITE PBR CODE
// TODO: switch the location of the metallic and roughness in the thingy
layout(std430) readonly buffer samplers {
    vec4 materialInstance[];
};

// MATERIAL_TYPE enum
#define MATERIAL_TYPE_DIFFUSE_SPECULAR 1 // standard cook-torrance BRDF
#define MATERIAL_TYPE_REFRACTIVE 2       // refractive materials
#define MATERIAL_TYPE_MIRROR 3           // perfect mirrors (unless I find a way how to do it in trowbridge-reitz without hitting numerical instabilities)
// End

// I will be following the naming scheme used by Walters et al. 2007 "Microfacet Models for Refraction through Rough Surfaces"

// Mapped to by material ID
struct MaterialInstance {
    // Weird variable ordering to pack stuff into vec4
    vec3 albedo;
    uint materialType; 

    vec3 emission;
    float metallic;

    vec3 reflectance;
    float ior;

    float roughness;
    float roughness2;
};

// Basically a constructor but the ugly C-way
MaterialInstance ConstructMaterialInstance(inout uint materialID, inout vec2 texcoord) {
    MaterialInstance material;
    material.emission = materialInstance[materialID + 1].xyz;

    vec4 data0 = texture(sampler2D(fbu(materialInstance[materialID].xy)), texcoord);
    vec4 data1 = texture(sampler2D(fbu(materialInstance[materialID].zw)), texcoord);

    material.albedo = data0.xyz;
    material.roughness = min(data1.g * data1.g, 1e-4f); // Solution to TR-GGX NaNs suggested by Jaker. Goodbye, perfect mirrors
    material.roughness2 = material.roughness * material.roughness;
    material.metallic = data1.b;

    material.reflectance = mix(vec3(0.0f), material.albedo, material.metallic);

    return material;
}

// Interaction between vectors on a surface
struct SurfaceInteraction {
    // the vectors that contain the properties of our interaction
    vec3 normal;     // surface normal. MUST EQUAL GEOMETRIC NORMAL
    float ndm;
    vec3 outgoing;   // view vecotr
    float ndo;
    vec3 incoming;   // light vector
    float ndi;
    vec3 microfacet; // "halfway" sounds like a really ugly variable name and almost has no context behind it
    float ndm2;
    mat3 tbn;
    float idm;
};

mat3 ConstructTBN(in vec3 normal) {
    vec3 normcrs = (abs(normal.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0));
    vec3 tangent = normalize(cross(normcrs, normal));
    vec3 bitangent = cross(tangent, normal);
    return mat3(tangent, bitangent, normal);
}

// Construct a surface interaction from scratch
SurfaceInteraction ConstructSurfaceInteraction(inout vec3 n, inout vec3 o, inout vec3 i) {
    SurfaceInteraction construction;

    construction.normal = n;
    construction.outgoing = o;
    construction.incoming = i;
    construction.microfacet = normalize(construction.outgoing + construction.incoming);

    construction.ndo  = nndot(construction.normal, construction.outgoing);
    construction.ndi  = nndot(construction.normal, construction.incoming);
    construction.ndm  = nndot(construction.normal, construction.microfacet);
    construction.ndm2 = construction.ndm * construction.ndm;
    construction.idm  = nndot(construction.incoming, construction.microfacet);

    construction.tbn = ConstructTBN(construction.normal);

    return construction;
}

// Construct a surface interaction partially
SurfaceInteraction ConstructSurfaceInteraction(inout vec3 n, inout vec3 o) {
    SurfaceInteraction construction;

    construction.normal = n;
    construction.outgoing = o;

    construction.ndo = nndot(construction.normal, construction.outgoing);
    construction.tbn = ConstructTBN(construction.normal);

    return construction;
}

// Set a new incoming direction a surface interaction, and create a new microfacet vector
void SetIncomingDirection(inout SurfaceInteraction interaction, in vec3 i) {
    interaction.incoming = i;
    interaction.microfacet = normalize(interaction.outgoing + interaction.incoming);

    interaction.ndi = nndot(interaction.normal, interaction.incoming);
    interaction.ndm = nndot(interaction.normal, interaction.microfacet);
    interaction.ndm2 = interaction.ndm * interaction.ndm;
    interaction.idm = nndot(interaction.incoming, interaction.microfacet);
}

// WARNING: everything below here is messy code. Read at your own peril

/*
// I'm using a simple BRDF instead of a proper one to make debugging easier 
// SURFERS FROM FLOATING POINT ACCURACY ISSUES AT HIGH SMOOTHNESS
vec3 GGX_CookTorrance(in MaterialInstance material, in vec3 n, in vec3 v, in vec3 l) {
    if (dot(n, v) < 0.0f || dot(n, l) < 0.0f) {
        return vec3(0.0f);
    }
    // Cook torrance
    vec3 f0 = mix(vec3(0.04f), material.albedo, material.metallic);
    vec3 h = normalize(v + l);
    vec3 specular = FresnelShlick(f0, h, v) * MicrofacetDistribution(n, h, material.roughness2) * GeometricShadowing(n, v, l, material.roughness) / max(4.0f * nndot(n, v) * nndot(n, l), 1e-26f);
    // Some changes to what devsh wrote and some ideas:
    // First of all, when calculating the diffuse, we need to be careful about the internal reflection constant
    // We need to use the refractive index to get an outgoing ray direction instead of using just v
    // Then we have to integrate that per each microfacet since they have different half vectors and thus can internally reflect a different amount of light
    // Finally, in the case of (total) internal reflection, some of the light that is reflected back inside is either absorbed or tries to scatter again
    // We need to account for this internal reflection or we will loose energy
    // Unfortunately I am no math genius and I do not want to change this without breaking some rule so I won't touch this but I will leave these thoughts here
    vec3 diffuse = material.albedo / M_PI;// *(1.0f - material.metallic)* (1.0f - FresnelShlick(f0, n, l))* (1.0f - FresnelShlick(f0, n, v)); // See pbr discussion by devsh on how to do energy conservation
    return specular + diffuse;
}*/



#endif

// Very old code:
/*
float DistributionTrowbridgeReitz(in vec3 n, in vec3 h, in float roughness) {
    float noh = max(dot(n, h), 0.0f);
    float a2 = roughness * roughness;
    float k = (noh * noh * (a2 - 1.0f) + 1.0f);
    float div = M_PI * k * k;
    return a2 / div;
}

float GeometryShlickGGX(vec3 n, vec3 v, float k) {
    float nov = max(dot(n, v), 0.0f);
    return nov / (nov * (1.0 - k) + k);
}

float GeometrySmith(in vec3 n, in vec3 v, in vec3 l, in float roughness) {
    float k = roughness + 1;
    k = k * k / 8;
    return GeometryShlickGGX(n, v, k) * GeometryShlickGGX(n, l, k);
}

float VisibilityGGX(in float nov, in float nol, in float a2) {
    return nol * sqrt(nov * nov * (1 - a2) + a2);
}

float VisibilitySmithGGXCorrelated(in vec3 n, in vec3 v, in vec3 l, in float roughness) {
    float a2 = roughness * roughness;
    float nov = max(dot(n, v), 0.0f);
    float nol = max(dot(n, l), 0.0f);
    float div = VisibilityGGX(nov, nol, a2) + VisibilityGGX(nol, nov, a2);
    return 0.5f / div;
}



// https://casual-effects.com/research/McGuire2013CubeMap/paper.pdf
vec3 BlinnPhongNormalized(in vec3 albedo, in float shiny, in vec3 specular, in vec3 n, in vec3 h) {
    float distribution = pow(max(dot(n, h), 0.0f), shiny) * (shiny + 8.0f) / (8.0); // http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
    return (albedo + specular * distribution) / M_PI;
}

vec3 BlinnPhongNormalizedPBR(in vec3 albedo, in float metallic, in float roughness, in vec3 n, in vec3 v, in vec3 l) {
    // GGX != beckman but this is the only remapping I know
    float shiny = 2 / (roughness * roughness) - 2;
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    vec3 h = normalize(v + l);
    return BlinnPhongNormalized(albedo * (1.0f - metallic), shiny, FresnelShlick(f0, h, v), n, h);
}

// https://docs.google.com/document/d/1ZLT1-fIek2JkErN9ZPByeac02nWipMbO89oCW2jxzXo/edit
vec3 SingleScatterCookTorrace(in vec3 albedo, in float metallic, in float roughness,  in vec3 n, in vec3 v, in vec3 l) {
    // If any point is bellow the hemisphere then do not reflect; BRDFs only work when both points are above the surface
    if (dot(n, v) < 0.0f || dot(n, l) < 0.0f) {
        return vec3(0.0f);
    }
    // Cook torrance
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    vec3 h = normalize(v + l);
    vec3 specular = DistributionTrowbridgeReitz(n, h, roughness) * VisibilitySmithGGXCorrelated(n, v, l, roughness) * FresnelShlick(f0, h, v) / max(4 * max(dot(n, v), 0.0f) * max(dot(n, l), 0.0f), 0.001f);
    // Energy conserving diffuse
    vec3 diffuse = (1.0 - FresnelShlick(f0, n, l)) * (1.0f - FresnelShlick(f0, n, v)) * albedo / M_PI;
    return specular + diffuse * (1.0 - metallic);
}

// https://schuttejoe.github.io/post/ggximportancesamplingpart1/
vec3 ImportanceSampleDistributionGGX(in float roughness, out float pdf) {
    float a2 = roughness * roughness;

    // Chose a direction https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
    float rand0 = rand(), rand1 = rand();
    float theta = atan(roughness * sqrt(rand0 / (1.0 - rand0))); // The article presents two methods to convert from rand to theta, one that uses acos and one that uses atan. acos causes nans, while atan does not
    float phi = 2 * M_PI * rand1;

    // Compute the direction
    vec3 direction;
    direction.x = sin(theta) * sin(phi);
    direction.y = sin(theta) * cos(phi);
    direction.z = cos(theta);

    // Calculate pdf
    float div = (a2 - 1) * cos(theta) * cos(theta) + 1;
    pdf = a2 * cos(theta) * sin(theta) / (M_PI * div * div);

    return direction;
}

float SamplePdfDistributionGGX(in vec3 n, in vec3 h, in float a) {
    float costheta = max(dot(n, h), 0.0f);
    float sintheta = sqrt(1.0 - costheta * costheta);
    float a2 = a * a;
    float div = (a2 - 1) * costheta * costheta + 1;
    float pdf = a2 * costheta* sintheta / (M_PI * div * div);
    return pdf;
}
*/
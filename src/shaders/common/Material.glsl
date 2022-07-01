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

//#define LOGL_PBR
#ifdef LOGL_PBR

float DistributionTrowbridgeReitz(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelShlick(vec3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelShlick(vec3 F0, vec3 n, vec3 d)
{
    return FresnelShlick(F0, max(dot(n, d), 0.0f));
}

#else

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

vec3 FresnelShlick(in vec3 f0, in vec3 n, in vec3 v) {
    float x = 1.0f - max(dot(n, v), 0.0f);
    return f0 + (1.0f - f0) * (x * x * x * x * x);
}

#endif

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

vec3 ReflectiveTest(in vec3 albedo, in float roughness, in float metallic, in vec3 n, in vec3 v, in vec3 l) {
    vec3 r = reflect(-l, n);
    float dist = pow(max(dot(r, v), 0.0f), 256.0f);
    return 1000000000.0 * albedo * dist;
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

float SamplePdfCosine(in vec3 n, in vec3 l) {
    return max(dot(n, l), 0.0f) / M_PI;
}

vec3 ImportanceSampleCosine(out float pdf) {
    vec2 r = rand2();
    float radius = sqrt(r.x);
    float phi = 2 * M_PI * r.y;
    float z = sqrt(1.0 - r.x);
    pdf = max(z / M_PI, 1e-10f);
    return vec3(radius * vec2(sin(phi), cos(phi)), z);
}

// http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
float DistributionBeckmannUNSTABLE(in vec3 n, in vec3 h, in float m) {
    float noh = max(dot(n, h), 0.0f);
    float noh2 = noh * noh;
    float m2 = m * m;
    float numer = exp((noh2 - 1.0f) / (m2 * noh2));
    float denom = M_PI * m2 * noh2 * noh2;
    return numer / denom;
}

// With a bit of highschool logarithm algebra we can make this significantly more stable in no time
float DistributionBeckmannSTABLE(in vec3 n, in vec3 h, in float m) {
    float noh = nndot(n, h);
    float noh2 = noh * noh;
    float sub = 2.0f * log(sqrt(M_PI) * m * noh);
    float add = (noh2 - 1.0f) / (noh2 * m * m);
    return exp(add - sub);
}

#define DistributionBeckmann(n, h, m) DistributionBeckmannSTABLE(n, h, m)

// See "Microfacet Models for Refraction through Rough Surfaces", eqs 28 and 29
// Interestingly, "A Microfacet Based Coupled Specular-Matte BRDF Model with Importance Sampling" complains importance sampling the beckmann equation isn't possible, so I might be reading the EGSR 2007 paper incorrectly
vec3 BeckmannImportanceSample(in float m) {
    vec2 r = rand2();
    float g = -m * m * log(1 - r.x);
    float z2 = 1.0f / (1.0f + g);
    float z = sqrt(z2);
    //pdf = DistributionBeckmann(vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, z), m);
    float phi = 2 * M_PI * r.y;
    float radius = sqrt(1.0 - z2);
    return vec3(radius * vec2(sin(phi), cos(phi)), z);
}

// pdf of selecting l given n,v and m is just the distribution since itself is a pdf
// Edit: https://www.gamedev.net/blogs/entry/2261786-microfacet-importance-sampling-for-dummies/
// My pdf was broken, which made me loose a day trying to render the refrence and seeing what would work
float PdfBeckmannH(in float m, in vec3 n, in vec3 v, in vec3 h) {
    return  max(DistributionBeckmann(n, h, m) * nndot(n, h) / (4.0f * nndot(v, h)), 1e-32f); // Introducing a little bit of PDF bias helps us avoid NaNs
}
float PdfBeckmann(in float m, in vec3 n, in vec3 v, in vec3 l) {
    vec3 h = normalize(v + l);
    return  PdfBeckmannH(m, n, v, h);
}

// See the simonstechblog article for this as well
float G1_Shlick(vec3 n, vec3 v, float k) {
    float nov = max(dot(n, v), 0.0f);
    return nov / (nov * (1.0 - k) + k);
}

float GSmith(vec3 n, vec3 v, vec3 l, float m) {
    float k = m + 1.0;
    k *= k / 8.0f;
    return G1_Shlick(n, v, k) * G1_Shlick(n, l, k);
}

// Move to GGX, suggested by adrian (thank you!)
float GGX_Distribution(in vec3 n, in vec3 h, in float a) {
    float a2 = a * a;
    float noh = nndot(n, h);
    float div = (a2 - 1.0f) * noh * noh + 1.0f;
    float num = a2;
    return num / (M_PI * div * div);
}

vec3 GGX_ImportanceSample(in float a) {
    float a2 = a * a;
    vec2 r = rand2();
    float nch = (1.0f - r.x) / (r.x * (a2 - 1.0f) + 1.0f);
    float nsh = sqrt(1.0f - nch * nch);

    r.y *= 2.0f * M_PI;

    vec3 direction;
    direction.x = nsh * sin(r.y);
    direction.y = nsh * cos(r.y);
    direction.z = nch;

    return direction;
}

float GGX_PDF(in vec3 n, in vec3 h, in float a) {
    float nch = nndot(n, h);
    float nsh = sqrt(1.0f - nch * nch);
    return max(nch * nsh * GGX_Distribution(n, h, a), 1e-20f);
}

float GGX_PDF(in vec3 n, in vec3 v, in vec3 l, in float a) {
    return GGX_PDF(n, normalize(v + l), a);
}

// I'm using a simple BRDF instead of a proper one to make debugging easier 
// SURFERS FROM FLOATING POINT ACCURACY ISSUES AT HIGH SMOOTHNESS
vec3 GGX_CookTorrance(in vec3 albedo, in float metallic, in float roughness, in vec3 n, in vec3 v, in vec3 l) {
    if (dot(n, v) < 0.0f || dot(n, l) < 0.0f) {
        return vec3(0.0f);
    }
    // Cook torrance
    vec3 f0 = mix(vec3(0.04f), albedo, metallic);
    vec3 h = normalize(v + l);
    vec3 specular = FresnelShlick(f0, h, v) * GGX_Distribution(n, h, roughness) * GSmith(n, v, l, roughness) / max(4.0f * nndot(n, v) * nndot(n, l), 1e-26f);
    vec3 diffuse = albedo / M_PI * (1.0f - metallic) * (1.0f - FresnelShlick(f0, n, l)) * (1.0f - FresnelShlick(f0, n, v)); // See pbr discussion by devsh on how to do energy conservation
    return specular + diffuse;
}

vec3 PhongTest(in vec3 albedo, in float metallic, in float roughness, in vec3 n, in vec3 v, in vec3 l) {
    vec3 idealReflect = reflect(-l, n);
    float distribution = (256.0f + 2.0) / (2*M_PI) * pow(nndot(idealReflect, v), 256.0f);
    return albedo * distribution;
}

#define BRDF(a, m, r, n, v, l) GGX_CookTorrance(a, m, r, n, v, l)

#endif
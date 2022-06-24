#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

layout(std430) readonly buffer samplers {
    vec4 materialInstance[];
};

// https://casual-effects.com/research/McGuire2013CubeMap/paper.pdf
vec3 BlinnPhongNormalized(in vec3 albedo, in float specular, in float shiny, in vec3 n, in vec3 v, in vec3 l) {
    return albedo / M_PI;
    vec3 h = normalize(v + l);
    float distribution = pow(max(dot(n, h), 0.0f), shiny);
    float reflectance = distribution * (shiny + 8.0f) / 8.0f;
    return (albedo + specular * reflectance) / M_PI;
}

#endif
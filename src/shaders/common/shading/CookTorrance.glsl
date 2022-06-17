#ifndef COOK_TORRANCE_GLSL
#define COOK_TORRANCE_GLSL

#include "Material.glsl"

const float kMathPi = 3.1415926535897932384626433832795028841971;

struct DirectionalParameters {
	vec3 Normal;

	vec3 View;
	vec3 Light;
	vec3 Halfway;

	float NdotV;
	float NdotL;

	float NdotH;
	float VdotH;

	float NdotH2;
};

DirectionalParameters ComputeDirectionalParameters(in vec3 Normal, in vec3 ViewDirection, in vec3 LightDirection) {
	DirectionalParameters DirParams;

	// Copied directions

	DirParams.Normal = Normal;
	DirParams.View   = ViewDirection;
	DirParams.Light  = LightDirection;

	// Dervied directions

	DirParams.Halfway = normalize(DirParams.View + DirParams.Light);

	// Dot products

	DirParams.NdotV = dot(DirParams.Normal, DirParams.View);
	DirParams.NdotL = dot(DirParams.Normal, DirParams.Light);

	DirParams.NdotH = dot(DirParams.Halfway, DirParams.Normal);
	DirParams.NdotH = dot(DirParams.Halfway, DirParams.View);
	
	// Derived dot products

	DirParams.NdotH2 = DirParams.NdotH * DirParams.NdotH;

	return DirParams;
}

float MicrofaceDistributionGGX(in float Roughness2, in float NdotH2) {
	float Div = (NdotH2 * (Roughness2 - 1.0f) + 1);
	Div *= kMathPi * Div;

	float Mfacet = Roughness2 / Div;

	return Mfacet;
}

float GeometicalAttenuationSchlickGGX(in float K, in float NdotD) {
	float G = NdotD / (NdotD * (1.0f - K) + K);

	return G;
}

float GeometicalAttenuationSmith(in float K, in float NdotV, in float NdotL) {
	float GV = GeometicalAttenuationSchlickGGX(K, NdotV);
	float GL = GeometicalAttenuationSchlickGGX(K, NdotL);

	return GV * GL;
}

vec3 FresnelSchlick(in vec3 BaseReflectivity, in float VdotH) {
	float Angular  = 1.0f - VdotH;

	float Angular2 = Angular * Angular;
	float Angular5 = Angular2 * Angular2 * Angular;
	
	vec3 AngularReflectivity = (1.0f - BaseReflectivity) * Angular5;

	return BaseReflectivity + AngularReflectivity;
}

vec3 ComputeBRDFCookTorrance(in MaterialProperties Mtl, in DirectionalParameters DirParams) {

	float MicrofacetDistribution = MicrofaceDistributionGGX(Mtl.Roughness2, DirParams.NdotH2);
	float GeometicalAttenuation  = GeometicalAttenuationSmith(Mtl.GeometryRemap, DirParams.NdotV, DirParams.NdotL);

	float ReflectingMicrofacets = MicrofacetDistribution * GeometicalAttenuation;

	vec3 Fresnel = FresnelSchlick(Mtl.ReflectivityFresnelPerpendicular, DirParams.VdotH);

	vec3 Reflection = Fresnel * ReflectingMicrofacets;
	vec3 BRDF = Reflection / (4.0f * DirParams.NdotV * DirParams.NdotL);

	return BRDF;

}

#endif
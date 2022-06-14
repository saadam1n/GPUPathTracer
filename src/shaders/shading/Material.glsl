#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

struct MaterialSamplers {
    sampler2D Diffuse;
    sampler2D Metallic;
    sampler2D Roughness;
    sampler2D Occlusion;
};

struct MaterialProperties {
	vec3 ReflectivityDiffuse;
	vec3 ReflectivityFresnelPerpendicular;
	float Metallic;
	float Roughness;
	float Roughness2;
	float GeometryRemap;
	float Occlusion;
};

MaterialProperties GetMaterialProperties(in MaterialSamplers Samplers, in vec2 TexCoords) {
	MaterialProperties Properties;

	// Sampled properties

	Properties.ReflectivityDiffuse = texture(Samplers.Diffuse, TexCoords).rgb;

	Properties.Metallic  = texture(Samplers.Metallic , TexCoords).r;
	Properties.Roughness = texture(Samplers.Roughness, TexCoords).r;
	Properties.Occlusion = texture(Samplers.Occlusion, TexCoords).r;

	// Derivied properties

	Properties.Roughness2 = Properties.Roughness * Properties.Roughness;

	Properties.GeometryRemap  = Properties.Roughness + 1;
	Properties.GeometryRemap *= Properties.GeometryRemap / 8.0f;

	Properties.ReflectivityFresnelPerpendicular = mix(vec3(0.04), Properties.ReflectivityDiffuse, Properties.Metallic);

	return Properties;
}

#endif
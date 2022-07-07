#ifndef BSDF_GLSL
#define BSDF_GLSL

#include "Material.glsl"
#include "Microfacet.glsl"

// Specular energy has gone wonkers
vec3 ComputeBSDF(in MaterialInstance material, in SurfaceInteraction interaction) {
    //if (dot(interaction.normal, interaction.incoming) < 0.0f || dot(interaction.normal, interaction.outgoing) < 0.0f) return vec3(0.0f);
    // Cook torrance
    vec3 specular = Fresnel(material, interaction.idm) * MicrofacetDistribution(material, interaction) * GeometricShadowing(material, interaction) / max(4.0f * interaction.ndi * interaction.ndo, 1e-26f);
    // Some changes to what devsh wrote and some ideas:
    // First of all, when calculating the diffuse, we need to be careful about the internal reflection constant
    // We need to use the refractive index to get an outgoing ray direction instead of using just v
    // Then we have to integrate that per each microfacet since they have different half vectors and thus can internally reflect a different amount of light
    // Finally, in the case of (total) internal reflection, some of the light that is reflected back inside is either absorbed or tries to scatter again
    // We need to account for this internal reflection or we will loose energy
    // Unfortunately I am no math genius and I do not want to change this without breaking some rule so I won't touch this but I will leave these thoughts here
    vec3 diffuse = material.albedo / M_PI * DiffuseEnergyConservation(material, interaction); // See pbr discussion by devsh on how to do energy conservation
    return specular + diffuse;
}

#endif
#version 330 core

in vec3 sampleCoords;

out vec3 color;

uniform sampler2D hdrTex;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
	color = texture(hdrTex, SampleSphericalMap(normalize(sampleCoords))).xyz;
}
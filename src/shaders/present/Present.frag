#version 330 core

in vec2 SampleCoords;

out vec3 color;

uniform sampler2D ColorTexture;
uniform float exposure;

void main(){
	color = texelFetch(ColorTexture, ivec2(gl_FragCoord.xy), 0).rgb;
	color = 1.0 - exp(-exposure * color);
	// TODO: More accurate sRGB conversion
    color = pow(color, vec3(1.0f / 2.2f));
}
#version 330 core

in vec2 SampleCoords;

out vec3 Color;

uniform sampler2D ColorTexture;

void main(){
	Color = texelFetch(ColorTexture, ivec2(gl_FragCoord.xy), 0).rgb;
}
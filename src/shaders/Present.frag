#version 330 core

in vec2 SampleCoords;

out vec3 color;

uniform sampler2D directAccum;
uniform float exposure;
uniform int numSamples;

#define TONEMAPPING

void main(){
	color =  texelFetch(directAccum, ivec2(gl_FragCoord.xy), 0).rgb / numSamples;
	#ifdef TONEMAPPING
	color = 1.0 - exp(-exposure * color);
	#endif
	// TODO: More accurate sRGB conversion
    color = pow(color, vec3(1.0f / 2.2f));
}

/*
	if(gl_FragCoord.x < 1280 / 2)
		color = texelFetch(indirectAccum, ivec2(gl_FragCoord.xy), 0).rgb / numSamples;
	else	
		color = texelFetch(directAccum, ivec2(gl_FragCoord.xy), 0).rgb / numSamples;
*/
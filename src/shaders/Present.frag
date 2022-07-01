#version 330 core

in vec2 SampleCoords;

out vec3 color;

uniform sampler2D directAccum;
uniform float exposure;
uniform int numSamples;

#define TONEMAPPING

vec3 ComputeTonemapUncharted2(vec3 color) {
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    float W = 11.2f;
    float exposure = 2.0f;
    color *= exposure;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    return color;
}

void main(){
	color =  texelFetch(directAccum, ivec2(gl_FragCoord.xy), 0).rgb / numSamples;
	#ifdef TONEMAPPING
	//color = 1.0 - exp(-exposure * color);
    //color = ComputeTonemapUncharted2(exposure * color);
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
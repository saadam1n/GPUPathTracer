#ifndef SKY_GLSL
#define SKY_GLSL

// Sky by builderboy
// Original coeffs: vec3(0.0625, 0.125, 0.25)
vec3 ComputeIncomingRadiance(in vec3 Direction) {
	float UpDot = Direction.y;
	const vec3 Coefficients = vec3(0.0625, 0.125, 0.25); //  0.1686, 0.4, 0.8667
	vec3 Color = Coefficients / (UpDot * UpDot + Coefficients);
	return Color;
}

#endif
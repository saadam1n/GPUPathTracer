#ifndef UTIL_GLSL
#define UTIL_GLSL

#define fbu(f) floatBitsToUint(f)
#define fbs(f) floatBitsToInt(f)
#define nndot(a, b) max(dot(a, b), 0.0f) // non-negative dot
#define avdot(a, b) abs(dot(a, b)) // absolute value dot

float AverageLuminanace(in vec3 v) {
	return dot(v, vec3(1.0f / 3.0f));
}

#endif
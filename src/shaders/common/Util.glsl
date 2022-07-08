#ifndef UTIL_GLSL
#define UTIL_GLSL

#define fbu(f) floatBitsToUint(f)
#define fbs(f) floatBitsToInt(f)
#define nndot(a, b) max(dot(a, b), 0.0f) // non-negative dot
#define avdot(a, b) abs(dot(a, b)) // absolute value dot

float AverageLuminance(in vec3 v) {
	return dot(v, vec3(1.0f / 3.0f));
}

// Morton order unpacking - taken from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

uint Compact1By1(uint x) {
    x &= 0x55555555;                
    x = (x ^ (x >> 1)) & 0x33333333;
    x = (x ^ (x >> 2)) & 0x0f0f0f0f;
    x = (x ^ (x >> 4)) & 0x00ff00ff;
    x = (x ^ (x >> 8)) & 0x0000ffff;
    return x;
}

uvec2 DecodeMortonCode(uint code) {
    return uvec2(Compact1By1(code), Compact1By1(code >> 1));
}


#endif
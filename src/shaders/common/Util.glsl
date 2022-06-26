#ifndef UTIL_GLSL
#define UTIL_GLSL

#define fbu(f) floatBitsToUint(f)
#define fbs(f) floatBitsToInt(f)
#define nndot(a, b) max(dot(a, b), 0.0f) // non-negative dot

#endif
#ifndef RANDOM_GLSL
#define RANDOM_GLSL

//RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)

//internal RNG state 
uvec4 seed;
ivec2 pixel;

void InitRNG(vec2 p, int frame)
{
    pixel = ivec2(p);
    seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

void pcg4d(inout uvec4 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
    v = v ^ (v >> 16u);
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
    pcg4d(seed); return float(seed.x) / float(0xffffffffu);
}

//uniform int FrameNum;
//InitRNG(vec2(gl_GlobalInvocationID.xy), FrameNum);

#endif
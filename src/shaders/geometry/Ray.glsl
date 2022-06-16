#ifndef RAY_GLSL
#define RAY_GLSL

struct Ray {
    vec3 Origin;
    vec3 Direction;
};

struct RayInfo {
    vec3 origin;
    vec3 direction;
    uvec2 pixel;
};

#endif
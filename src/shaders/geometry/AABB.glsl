#ifndef AABB_GLSL
#define AABB_GLSL

#include "Ray.glsl"

struct AABB {
    vec3 Min;
    vec3 Max;
};

float IntersectPlane(in vec3 Origin, in vec3 Normal, in Ray Ray) {
    float DdotN = dot(Ray.Direction, Normal);
    vec3 TransformedOrigin = Origin - Ray.Origin;
    float TdotN = dot(TransformedOrigin, Normal);
    return TdotN / DdotN;
}

vec2 IntersectionAABBDistance(in AABB Box, in Ray Ray, uint Index) {
    vec2 Positions = vec2(Box.Min[Index], Box.Max[Index]);
    vec2 Distances = Positions / Ray.Direction[Index];
    if (Distances.x > Distances.y) {
        float Temp = Distances.x;
        Distances.x = Distances.y;
        Distances.y = Temp;
    }
    return Distances;
}

bool IntersectAABB(in AABB Box, in Ray Ray) {

    Box.Max -= Ray.Origin;
    Box.Min -= Ray.Origin;

    bool Result = false;

    vec2 T[3];

    T[0] = IntersectionAABBDistance(Box, Ray, 0);
    T[1] = IntersectionAABBDistance(Box, Ray, 1);
    T[2] = IntersectionAABBDistance(Box, Ray, 2);

    float tmin = T[0].x, tmax = T[0].y;
    float tymin = T[1].x, tymax = T[1].y;
    float tzmin = T[2].x, tzmax = T[2].y;

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    if (tzmax < tmax)
        tmax = tzmax;

    return Result;
}

bool IntersectAABB2(in AABB Box, in Ray Ray) {
    vec3 InverseDirection = 1.0f / Ray.Direction;

    vec3 tbot = InverseDirection * (Box.Min - Ray.Origin);
    vec3 ttop = InverseDirection * (Box.Max - Ray.Origin);

    vec3 tmin = min(ttop, tbot);
    vec3 tmax = max(ttop, tbot);

    vec2 t = max(tmin.xx, tmin.yz);
    float t0 = max(t.x, t.y);

    t = min(tmax.xx, tmax.yz);
    float t1 = min(t.x, t.y);

    return t1 > max(t0, 0.0);
}

#endif
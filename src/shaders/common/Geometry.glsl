#ifndef GEOMETRY_GLSL
#define GEOMETRY_GLSL

struct Ray {
    vec3 Origin;
    vec3 Direction;
};

struct RayInfo {
    vec3 origin;
    vec3 direction;
    uvec2 pixel;
};

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

struct Vertex {
    vec3 Position;
    vec3 Normal;
    vec2 TextureCoordinate;
    int MatID;
};

// Workaround for vec4 alignment
struct PackedVertex {
    // Position xyz, normal x
    vec4 PN;
    // Normal yz, tex coord xy
    vec4 NT;
    // Material ID and 3 ints of padding (a tangent in the future, maybe?)
    vec4 MP;
};



Vertex UnpackVertex(in PackedVertex PV) {
    Vertex Unpacked;

    Unpacked.Position = PV.PN.xyz;

    Unpacked.Normal.x = PV.PN.w;
    Unpacked.Normal.yz = PV.NT.xy;

    Unpacked.TextureCoordinate = PV.NT.zw;

    Unpacked.MatID = floatBitsToInt(PV.MP.x);

    return Unpacked;
}

struct Triangle {
    PackedVertex[3] Vertices;
};

struct VertexInterpolationInfo {
    float U, V;
};

struct TriangleHitInfo {
    Triangle IntersectedTriangle;
    VertexInterpolationInfo InterpolationInfo;
};

struct HitInfo {
    vec4 di;
    Triangle intersected;
};


bool IntersectTriangle(in Triangle tri, in Ray ray, inout HitInfo closestHit) {
    // this is mostly a copy paste from scratchapixel's code that has been refitted to work with GLSL
    HitInfo attemptHit;
    attemptHit.intersected = tri;

    vec3 v01 = tri.Vertices[1].PN.xyz - tri.Vertices[0].PN.xyz;
    vec3 v02 = tri.Vertices[2].PN.xyz - tri.Vertices[0].PN.xyz;

    vec3 p = cross(ray.Direction, v02);

    float det = dot(v01, p);

    //#define AVOID_DIV_BY_0
#ifdef AVOID_DIV_BY_0
    if (abs(det) < 1e-6f) {
        return false;
    }
#endif

    float idet = 1.0f / det;

    vec3 t = ray.Origin - tri.Vertices[0].PN.xyz;
    attemptHit.di.y = dot(t, p) * idet;

    if (attemptHit.di.y < 0.0f || attemptHit.di.y > 1.0f) {
        return false;
    }

    vec3 q = cross(t, v01);
    attemptHit.di.z = dot(ray.Direction, q) * idet;

    if (attemptHit.di.z < 0.0f || attemptHit.di.y + attemptHit.di.z  > 1.0f) {
        return false;
    }

    attemptHit.di.x = dot(v02, q) * idet;

    if (attemptHit.di.x < closestHit.di.x && attemptHit.di.x > 0.0f) {
        closestHit = attemptHit;
        return true;
    }
    else
        return false;
}

#include "Util.glsl"

Vertex GetInterpolatedVertex(in Ray ray, inout HitInfo intersection) {
    Vertex interpolated;

    // I should probably precompute 1 - U - V
    intersection.di.w = 1.0 - intersection.di.y - intersection.di.z;
    

    interpolated.Position = ray.Origin + ray.Direction * intersection.di.x;

    /*
    interpolated.Normal = normalize(

        vec3(intersection.intersected.Vertices[1].PN.w, intersection.intersected.Vertices[1].NT.xy) * intersection.di.y +
        vec3(intersection.intersected.Vertices[2].PN.w, intersection.intersected.Vertices[2].NT.xy) * intersection.di.z +
        vec3(intersection.intersected.Vertices[0].PN.w, intersection.intersected.Vertices[0].NT.xy) * intersection.di.w);
        */

    // It is better to use this since in path tracing, the normal MUST be perpendicular to the triangle face to conserve energy
    interpolated.Normal = vec3(intersection.intersected.Vertices[0].PN.w, intersection.intersected.Vertices[0].NT.xy);

    
    interpolated.TextureCoordinate =

        intersection.intersected.Vertices[1].NT.zw * intersection.di.y +
        intersection.intersected.Vertices[2].NT.zw * intersection.di.z +
        intersection.intersected.Vertices[0].NT.zw * intersection.di.w;


    interpolated.MatID = fbs(intersection.intersected.Vertices[0].MP.x);

    return interpolated;
}

uniform samplerBuffer vertexTex;
uniform isamplerBuffer indexTex;

#define FetchIndexData(idx) texelFetch(indexTex, idx).xyz

PackedVertex FetchVertex(uint uidx) {
    PackedVertex PV;

    int idx = 3 * int(uidx);
    PV.PN = texelFetch(vertexTex, idx);
    PV.NT = texelFetch(vertexTex, idx + 1);
    PV.MP = texelFetch(vertexTex, idx + 2);

    return PV;
}

Triangle FetchTriangle(ivec3 indices) {
    Triangle CurrentTriangle;

    CurrentTriangle.Vertices[0] = FetchVertex(indices[0]);
    CurrentTriangle.Vertices[1] = FetchVertex(indices[1]);
    CurrentTriangle.Vertices[2] = FetchVertex(indices[2]);

    return CurrentTriangle;
}

#endif
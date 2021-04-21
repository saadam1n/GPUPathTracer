#ifndef TRIANGLE_GLSL
#define TRIANGLE_GLSL

#include "Ray.glsl"

struct Vertex {
    vec3 Position;
    vec3 Normal;
    vec2 TextureCoordinate;
};

// Workaround for vec4 alignment
struct PackedVertex {
    // Position xyz, normal x
    vec4 PN;
    // Normal yz, tex coord xy
    vec4 NT;
};

Vertex UnpackVertex(in PackedVertex PV) {
    Vertex UnpackedVertex;

    UnpackedVertex.Position = PV.PN.xyz;

    UnpackedVertex.Normal.x = PV.PN.w;
    UnpackedVertex.Normal.yz = PV.NT.xy;

    UnpackedVertex.TextureCoordinate = PV.NT.zw;

    return UnpackedVertex;
}

struct Triangle {
    Vertex[3] Vertices;
};

struct VertexInterpolationInfo {
    float U, V;
};

struct TriangleHitInfo {
    Triangle IntersectedTriangle;
    VertexInterpolationInfo InterpolationInfo;
};

struct HitInfo {
    float Depth;
    TriangleHitInfo TriangleHitInfo;
};

struct TriangleIndexData {
    uvec3 Indices;
};


bool IntersectTriangle(in Triangle Triangle, in Ray Ray, inout HitInfo Hit) {
    // this is mostly a copy paste from scratchapixel's code that has been refitted to work with GLSL

    TriangleHitInfo TentativeTriangleHitInfo;
    TentativeTriangleHitInfo.IntersectedTriangle = Triangle;

    vec3 V01 = Triangle.Vertices[1].Position.xyz - Triangle.Vertices[0].Position.xyz;
    vec3 V02 = Triangle.Vertices[2].Position.xyz - Triangle.Vertices[0].Position.xyz;

    vec3 Pvec = cross(Ray.Direction, V02);

    float Det = dot(V01, Pvec);

#define AVOID_DIV_BY_0

#ifdef AVOID_DIV_BY_0
    const float Epsilon = 1e-6f;

    //#define BACK_FACE_CULLING

    float CompareVal =
#ifdef BACK_FACE_CULLING
        Det
#else
        abs(Det)
#endif
        ;

    if (CompareVal < Epsilon) {
        return false;
    }
#endif

    float InverseDet = 1.0f / Det;

    vec3 Tvec = Ray.Origin - Triangle.Vertices[0].Position;
    TentativeTriangleHitInfo.InterpolationInfo.U = dot(Tvec, Pvec) * InverseDet;

    if (TentativeTriangleHitInfo.InterpolationInfo.U < 0.0f || TentativeTriangleHitInfo.InterpolationInfo.U > 1.0f) {
        return false;
    }

    vec3 Qvec = cross(Tvec, V01);
    TentativeTriangleHitInfo.InterpolationInfo.V = dot(Ray.Direction, Qvec) * InverseDet;

    if (TentativeTriangleHitInfo.InterpolationInfo.V < 0.0f || TentativeTriangleHitInfo.InterpolationInfo.U + TentativeTriangleHitInfo.InterpolationInfo.V  > 1.0f) {
        return false;
    }

    float t = dot(V02, Qvec) * InverseDet;

    if (t < Hit.Depth && t > 0.0f) {
        Hit.Depth = t;
        Hit.TriangleHitInfo = TentativeTriangleHitInfo;
        return true;
    } else {
        return false;
    }
}

Vertex GetInterpolatedVertex(in HitInfo Intersection) {
    Vertex InterpolatedVertex;

    // I should probably precompute 1 - U - V

    InterpolatedVertex.Position =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].Position * Intersection.TriangleHitInfo.InterpolationInfo.U +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].Position * Intersection.TriangleHitInfo.InterpolationInfo.V +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].Position * (1.0f - Intersection.TriangleHitInfo.InterpolationInfo.U - Intersection.TriangleHitInfo.InterpolationInfo.V)

        ;

    InterpolatedVertex.Normal =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].Normal * Intersection.TriangleHitInfo.InterpolationInfo.U +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].Normal * Intersection.TriangleHitInfo.InterpolationInfo.V +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].Normal * (1.0f - Intersection.TriangleHitInfo.InterpolationInfo.U - Intersection.TriangleHitInfo.InterpolationInfo.V)

        ;

    InterpolatedVertex.TextureCoordinate =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].TextureCoordinate * Intersection.TriangleHitInfo.InterpolationInfo.U +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].TextureCoordinate * Intersection.TriangleHitInfo.InterpolationInfo.V +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].TextureCoordinate * (1.0f - Intersection.TriangleHitInfo.InterpolationInfo.U - Intersection.TriangleHitInfo.InterpolationInfo.V)

        ;

    InterpolatedVertex.Normal = normalize(InterpolatedVertex.Normal);

    return InterpolatedVertex;
}

#endif
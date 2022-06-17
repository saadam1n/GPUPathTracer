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
    // Material ID and 3 ints of padding
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

    //#define AVOID_DIV_BY_0

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
    }
    else {
        return false;
    }
}

Vertex GetInterpolatedVertex(in HitInfo Intersection) {
    Vertex InterpolatedVertex;

    // I should probably precompute 1 - U - V

    vec3 Interpolation = vec3(
        (1.0f - Intersection.TriangleHitInfo.InterpolationInfo.U - Intersection.TriangleHitInfo.InterpolationInfo.V),
        Intersection.TriangleHitInfo.InterpolationInfo.U,
        Intersection.TriangleHitInfo.InterpolationInfo.V
    );

    InterpolatedVertex.Position =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].Position * Interpolation[1] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].Position * Interpolation[2] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].Position * Interpolation[0]

        ;

    InterpolatedVertex.Normal =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].Normal * Interpolation[1] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].Normal * Interpolation[2] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].Normal * Interpolation[0]

        ;

    InterpolatedVertex.TextureCoordinate =

        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[1].TextureCoordinate * Interpolation[1] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[2].TextureCoordinate * Interpolation[2] +
        Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].TextureCoordinate * Interpolation[0]

        ;

    InterpolatedVertex.Normal = normalize(InterpolatedVertex.Normal);
    InterpolatedVertex.MatID = Intersection.TriangleHitInfo.IntersectedTriangle.Vertices[0].MatID;

    return InterpolatedVertex;
}

uniform samplerBuffer vertexTex;
uniform isamplerBuffer indexTex;

uint GetTriangleCount() {
    return textureSize(indexTex);
}

TriangleIndexData FetchIndexData(uint uidx) {
    TriangleIndexData TriangleIDX;

    TriangleIDX.Indices = texelFetch(indexTex, int(uidx)).xyz;

    return TriangleIDX;
}

Vertex FetchVertex(uint uidx) {
    PackedVertex PV;

    int idx = 3 * int(uidx);
    PV.PN = texelFetch(vertexTex, idx);
    PV.NT = texelFetch(vertexTex, idx + 1);
    PV.MP = texelFetch(vertexTex, idx + 2);

    return UnpackVertex(PV);
}

Triangle FetchTriangle(TriangleIndexData TriIDX) {
    Triangle CurrentTriangle;

    CurrentTriangle.Vertices[0] = FetchVertex(TriIDX.Indices[0]);
    CurrentTriangle.Vertices[1] = FetchVertex(TriIDX.Indices[1]);
    CurrentTriangle.Vertices[2] = FetchVertex(TriIDX.Indices[2]);

    return CurrentTriangle;
}

Triangle FetchTriangle(uint IDX) {
    return FetchTriangle(FetchIndexData(IDX));
}

#endif
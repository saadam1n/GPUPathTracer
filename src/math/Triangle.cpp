#include "Triangle.h"

using namespace glm;

/*
uint32_t& TriangleIndices::operator[](const uint32_t I) {
    return Indices[I];
}
*/

Vertex& Triangle::operator[](const uint32_t I) {
    return Vertices[I];
}

bool Triangle::Intersect(const Ray& ray, HitInfo& closestHit) {
    // this is mostly a copy paste from scratchapixel's code that has been refitted to work with GLSL
    HitInfo attemptHit;

    vec3 v01 = Vertices[1].position - Vertices[0].position;
    vec3 v02 = Vertices[2].position - Vertices[0].position;

    vec3 p = cross(ray.direction, v02);

    float det = dot(v01, p);
    float idet = 1.0f / det;

    vec3 t = ray.origin - Vertices[0].position;
    attemptHit.u = dot(t, p) * idet;

    if (attemptHit.u < 0.0f || attemptHit.u > 1.0f)
        return false;

    vec3 q = cross(t, v01);
    attemptHit.v = dot(ray.direction, q) * idet;

    if (attemptHit.v < 0.0f || attemptHit.u + attemptHit.v  > 1.0f)
        return false;

    attemptHit.depth = dot(v02, q) * idet;

    if (attemptHit.depth < closestHit.depth && attemptHit.depth > 0.0f) {
        closestHit = attemptHit;
        closestHit.intersection.position = closestHit.depth * ray.direction + ray.origin;
        closestHit.intersection.normal = Vertices[0].normal;
        closestHit.intersection.texcoords = Vertices[0].texcoords * closestHit.t + Vertices[1].texcoords * closestHit.u + Vertices[2].texcoords * closestHit.v;
        closestHit.intersection.matId = Vertices[0].matId;

        return true;
    }
    else
        return false;
}
#pragma once

#include <stdint.h>
#include "Vertex.h"
#include "Ray.h"

struct Triangle : public Hittable{
    Vertex Vertices[3];
    Vertex& operator[](const uint32_t I);
    bool Intersect(const Ray& ray, HitInfo& hit);
};

struct CompactTriangle : public Hittable {
    vec3 position0; // 3
    vec3 position1; // 6
    vec3 position2; // 9

    vec2 texcoord0; // 11
    vec2 texcoord1; // 13
    vec2 texcoord2; // 15

    vec3 normal; // 18

    uint32_t material; // 19
    uint32_t padding; // 20 - make 4 unit alignment

    Triangle Decompress();
    bool Intersect(const Ray& ray, HitInfo& hit);
};

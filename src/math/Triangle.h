#pragma once

#include <stdint.h>
#include "Vertex.h"
#include "Ray.h"

struct Triangle : public Hittable{
    Vertex Vertices[3];
    Vertex& operator[](const uint32_t I);
    bool Intersect(const Ray& ray, HitInfo& hit);
};

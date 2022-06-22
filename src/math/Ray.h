#pragma once

#include "Vertex.h"
#include <glm/glm.hpp>
using namespace glm;

struct Ray {
	vec3 origin;
	vec3 direction;
};

struct HitInfo {
    float depth;
    float u, v, t;
    Vertex intersection;
    HitInfo() : depth(1e20f) {}
};

struct Hittable {
    bool Intersect(const Ray& ray, HitInfo& hit) = delete;
};
#pragma once

#include <stdint.h>
#include "Vertex.h"

struct HitInfo;

/*
struct TriangleIndices {
    uint32_t Indices[3];
    uint32_t& operator[](const uint32_t I);
};
*/

struct Triangle {
    Vertex Vertices[3];
    Vertex& operator[](const uint32_t I);
};

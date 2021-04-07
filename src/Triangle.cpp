#include "Triangle.h"

/*
uint32_t& TriangleIndices::operator[](const uint32_t I) {
    return Indices[I];
}
*/

Vertex& Triangle::operator[](const uint32_t I) {
    return Vertices[I];
}

#include "TriangleIndexing.h"

uint32_t& TriangleIndexData::operator[](uint32_t Index) {
	return Indices[Index];
}


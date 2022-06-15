#pragma once

#include <vector>
#include <stdint.h>

struct TriangleIndexData {
	uint32_t Indices[3];
	uint32_t padding;
	//uint32_t Padding;
	uint32_t& operator[](uint32_t Index);
};

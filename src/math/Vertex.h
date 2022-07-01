#pragma once

#include <glm/glm.hpp>

struct Vertex {
	Vertex();

	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
	int matId;
	float area;

	int Padding[2];

	Vertex operator*(float Value);
	Vertex operator+(const Vertex& Value);
};
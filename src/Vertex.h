#pragma once

#include <glm/glm.hpp>

struct Vertex {
	Vertex(void) = default;

	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TextureCoordinates;
	int MatID;

	int Padding[3];

	Vertex operator*(float Value);
	Vertex operator+(const Vertex& Value);
};
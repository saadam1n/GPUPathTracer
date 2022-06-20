#include "Vertex.h"


Vertex Vertex::operator*(float Value) {
	Vertex NewVertex;

	NewVertex.position = position * Value;
	NewVertex.normal = normal * Value;
	NewVertex.texcoords = texcoords * Value;

	return NewVertex;
}

Vertex Vertex::operator+(const Vertex& Value) {
	Vertex NewVertex;

	NewVertex.position = position + Value.position;
	NewVertex.normal = normal + Value.normal;
	NewVertex.texcoords = texcoords + Value.texcoords;

	return NewVertex;
}
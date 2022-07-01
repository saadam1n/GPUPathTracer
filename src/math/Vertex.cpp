#include "Vertex.h"

Vertex::Vertex() : matId(0) {}

Vertex Vertex::operator*(float Value) {
	Vertex NewVertex;

	NewVertex.position = position * Value;
	NewVertex.normal = normal * Value;
	NewVertex.texcoord = texcoord * Value;

	return NewVertex;
}

Vertex Vertex::operator+(const Vertex& Value) {
	Vertex NewVertex;

	NewVertex.position = position + Value.position;
	NewVertex.normal = normal + Value.normal;
	NewVertex.texcoord = texcoord + Value.texcoord;

	return NewVertex;
}
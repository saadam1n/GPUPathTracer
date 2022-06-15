#include "Vertex.h"


Vertex Vertex::operator*(float Value) {
	Vertex NewVertex;

	NewVertex.Position = Position * Value;
	NewVertex.Normal = Normal * Value;
	NewVertex.TextureCoordinates = TextureCoordinates * Value;

	return NewVertex;
}

Vertex Vertex::operator+(const Vertex& Value) {
	Vertex NewVertex;

	NewVertex.Position = Position + Value.Position;
	NewVertex.Normal = Normal + Value.Normal;
	NewVertex.TextureCoordinates = TextureCoordinates + Value.TextureCoordinates;

	return NewVertex;
}
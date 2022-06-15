#ifndef OBJECT_GLSL
#define OBJECT_GLSL

#include "Triangle.glsl"

layout(std430) buffer vertexBuf {
	PackedVertex vertices[];
};

layout(std430) buffer indexBuf {
	uvec4 indices[];
};

uint GetTriangleCount() {
	return indices.length();
}

TriangleIndexData FetchIndexData(uint TriangleIndex) {
	TriangleIndexData TriangleIDX;

	TriangleIDX.Indices = indices[TriangleIndex].xyz;

	return TriangleIDX;
}

Vertex FetchVertex(uint IDX){
	PackedVertex PV = vertices[IDX];

	return UnpackVertex(PV);
} 

Triangle FetchTriangle(TriangleIndexData TriIDX) {
    Triangle CurrentTriangle;

	CurrentTriangle.Vertices[0] = FetchVertex(TriIDX.Indices[0]);
	CurrentTriangle.Vertices[1] = FetchVertex(TriIDX.Indices[1]);
	CurrentTriangle.Vertices[2] = FetchVertex(TriIDX.Indices[2]);

	return CurrentTriangle;
}

Triangle FetchTriangle(uint IDX) {
	return FetchTriangle(FetchIndexData(IDX));
}

#endif
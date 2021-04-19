#ifndef OBJECT_GLSL
#define OBJECT_GLSL

#include "Triangle.glsl"

struct MeshSamplers {
	samplerBuffer Vertices;
	usamplerBuffer Indices;
};

uint GetTriangleCount(in MeshSamplers M) {
	return textureSize(M.Indices);
}

TriangleIndexData FetchIndexData(in MeshSamplers M, uint TriangleIndex) {
	TriangleIndexData TriangleIDX;

	TriangleIDX.Indices = texelFetch(M.Indices, int(TriangleIndex)).xyz;

	return TriangleIDX;
}

Vertex FetchVertex(in MeshSamplers M, uint IDX){
	IDX *= 2;

	PackedVertex PV;

	PV.PN = texelFetch(M.Vertices, int(IDX    ));
	PV.NT = texelFetch(M.Vertices, int(IDX + 1));

	return UnpackVertex(PV);
} 

Triangle FetchTriangle(in MeshSamplers M, TriangleIndexData TriIDX) {
    Triangle CurrentTriangle;

	CurrentTriangle.Vertices[0] = FetchVertex(M, TriIDX.Indices[0]);
	CurrentTriangle.Vertices[1] = FetchVertex(M, TriIDX.Indices[1]);
	CurrentTriangle.Vertices[2] = FetchVertex(M, TriIDX.Indices[2]);

	return CurrentTriangle;
}

Triangle FetchTriangle(in MeshSamplers M, uint IDX) {
	return FetchTriangle(M, FetchIndexData(M, IDX));
}

#endif
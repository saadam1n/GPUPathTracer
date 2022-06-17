#ifndef OBJECT_GLSL
#define OBJECT_GLSL

#include "Triangle.glsl"

samplerBuffer vertexTex;
isamplerBuffer indexTex;

uint GetTriangleCount() {
	return textureSize(indexTex);
}

TriangleIndexData FetchIndexData(uint uidx) {
	TriangleIndexData TriangleIDX;

	TriangleIDX.Indices = texelFetch(indexTex, int(uidx)).xyz;

	return TriangleIDX;
}

Vertex FetchVertex(uint uidx){
	PackedVertex PV;

	int idx = 3 * int(uidx);
	PV.PN = texelFetch(vertexTex, idx    );
	PV.NT = texelFetch(vertexTex, idx + 1);
	PV.MP = texelFetch(vertexTex, idx + 2);

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
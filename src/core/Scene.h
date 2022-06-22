#pragma once

#include "BVH.h"
#include "Texture.h"
#include "Buffer.h"
#include <string>
#include <memory>
#include <glm/glm.hpp>
using namespace glm;

// Mapped to by material ID
struct MaterialInstance {
	GLuint64 samplerHandle;
	float ior;
	float padding0;
	vec3 emission;
	int isEmissive;
};

class Scene {
public:
	void LoadScene(const std::string& path, TextureCubemap* env_path);
private:
	std::vector<Vertex> vertexVec;
	Buffer vertexBuf;
	TextureBuffer vertexTex;

	std::vector<TriangleIndexData> indexVec;
	Buffer indexBuf;
	TextureBuffer indexTex;

	BoundingVolumeHierarchy bvh;

	// We never actually use the texture names after initialization but I keep them anyway
	std::vector<Texture*> textures;
	std::vector<MaterialInstance> materialVec;
	Buffer materialsBuf;

	float totalLightArea;
	Buffer lightBuf;
	TextureBuffer lightTex;

	friend class Shader;
	friend class Renderer;
};

/*
	Buffer VertexBuffer;
	Buffer ElementBuffer;

	struct {
		TextureBuffer Vertices;
		TextureBuffer Indices;
	} BufferTexture;

	BoundingVolumeHierarchy BVH;

	//AABB BoundingBox;

	struct {
		Texture2D Diffuse;
	} Material;
*/
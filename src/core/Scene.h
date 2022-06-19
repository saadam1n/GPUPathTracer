#pragma once

#include "BVH.h"
#include "Texture.h"
#include "Buffer.h"
#include <string>
#include <memory>

class Scene {
public:
	void LoadScene(const std::string& Path);
private:
	Buffer vertexBuf;
	TextureBuffer vertexTex;

	Buffer indexBuf;
	TextureBuffer indexTex;

	BoundingVolumeHierarchy bvh;

	// We never actually use the texture names after initialization but I keep them anyway
	std::vector<std::shared_ptr<Texture2D>> textures;
	Buffer materialsBuf;

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
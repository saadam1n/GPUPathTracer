#pragma once

#include "VertexArray.h"
#include "Buffer.h"

#include <glm/glm.hpp>

struct AABB {
	glm::vec3 Position;
	glm::vec3 Min;
	glm::vec3 Max;
};

class Mesh {
public:
	void LoadMesh(const char* Path);
private:
	/*
	For now, I will follow a simple layout where each mesh has it's own VAO. 
	
	However, I eventually should move to the format like in "Approaching Zero Driver Overhead" presentation
	In a nutshell, their method is to put the entire scene into one VAO
	Then a mesh object refrences a range of verticies in the vertex VBO

	Their approach can help not only if I want to rasterize the primary rays (in the context of a pin hole camera),
	but their approach also helps if I want to stuff every single object into one compute dispatch
	Not only does it lower the number of dispatches, I also avoid fewer read and write operations to memory like the output images
	*/

	Buffer VertexBuffer;
	Buffer ElementBuffer;

	AABB BoundingBox;

	friend class Shader;
};
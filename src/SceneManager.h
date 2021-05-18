#pragma once

#include "Mesh.h"

class SceneManager {
public:
	void LoadScene(const char* Path);

	std::vector<Mesh>::iterator StartMeshIterator(void);
	std::vector<Mesh>::iterator StopMeshIterator (void);
private:
	//struct Node {
	//	Mesh Object;
	//	std::vector<Node> Subnodes;
	//};

	std::vector<Mesh> Meshes;
};
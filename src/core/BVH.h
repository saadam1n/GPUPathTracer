#pragma once

#include "Buffer.h"
#include "Texture.h"
#include "../math/Triangle.h"
#include "../math/TriangleIndexing.h"
#include "../math/AABB.h"

#include <vector>
#include <stdint.h>

#include <glm/glm.hpp>

using namespace glm;

struct LeafContents {
	std::vector<uint32_t> Indices;
};

struct LeafPointer {
	int32_t Offset;
	int32_t Size;
};

enum class NodeType {
	LEAF,
	NODE
};

#define MAX_LEAF_TRIANGLES 15
struct NodeSerialized : public Hittable {

	void MakeLeaf(void);
	NodeType GetType(void);

	AABB BoundingBox;

	union {
		uint32_t firstChild; // 1 bit leaf flag
		uint32_t triangleRange; // 1 bit leaf flag, 27 bits triangle array offset, 4 bits triangle subarray length (15 triangles max per leaf)
	};

	int32_t parent;

	bool Intersect(const Ray& ray, HitInfo& hit, const std::vector<CompactTriangle>& triangles);
};

// BVH triangle
struct TriangleCentroid {
	glm::vec3 Position;
	uint32_t Index;
};

struct NodeUnserialized {
	NodeUnserialized(void);

	AABB BoundingBox;

	std::vector<TriangleCentroid> Centroids;

	NodeUnserialized* parent;
	//union {
		NodeUnserialized* Children[2];
		LeafPointer Leaf;
	//};

	NodeType Type;
	int32_t Index;

	uint32_t SplitAxis;

	float ComputeSAH(void);
};

struct Split {
	Split(void);
	// Axis of the split. 0-X, 1-Y, 2-Z
	uint32_t Axis;
	// List of centroids. 0 is behind of the split, 1 is infornt of it
	std::vector<TriangleCentroid> Centroids[2];
	// Boxes formed by the split
	AABB Box[2];
	// SAH score of the split
	float SAH;

	float ComputeSAH(void);
};

class BoundingVolumeHierarchy {
public:
	void SweepSAH(std::vector<CompactTriangle>& triangles);
	void BinnedSAH(std::vector<CompactTriangle>& triangles);
private:
	friend class Shader;
	friend class Renderer;

	std::vector<NodeSerialized> nodesVec;
	Buffer nodesBuf;
	TextureBuffer nodesTex;
	
	Buffer referenceBuf;
	TextureBuffer referenceTex;

	//AABB CreateBoundingBox(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes);

	//std::vector<TriangleCentroid> SortAxis(const std::vector<TriangleCentroid>& Centroids, uint32_t Axis);
	
	//Split CreateSplit(const std::vector<TriangleCentroid>& SortedCentroids, const std::vector<AABB>& TriangleBoundingBoxes, size_t CentroidIdx);

	//Split FindBestSplit(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes, uint32_t Axis);
	//Split ChooseBestSplit(const Split& X, const Split& Y, const Split& Z);

	//void RefineSplit(Split& BestSplit, const std::vector<TriangleCentroid>& Centroids, const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices);

	//void DebugPrintBVH(const std::vector<NodeSerialized>& Nodes, const std::vector<int32_t>& LeafContents);
};


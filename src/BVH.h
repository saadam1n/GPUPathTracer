#pragma once

#include "Buffer.h"
#include "Texture.h"
#include "Triangle.h"
#include "TriangleIndexing.h"
#include "AABB.h"

#include <vector>
#include <stdint.h>

#include <glm/glm.hpp>

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

struct NodeSerialized {

	void MakeLeaf(void);
	NodeType GetType(void);

	AABB BoundingBox;

	union {
		int32_t ChildrenNodes[2];
		LeafPointer Leaf;
	};
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
	void ConstructAccelerationStructure(const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices, uint32_t DepthHint, uint32_t LeafHint);
private:
	friend class Shader;

	// Depth of the BVH
	uint32_t Depth;

	// the binary large objects that contains BVH data
	struct {
		// TODO: compact these into a single buffer
		Buffer Nodes ;
		Buffer Leaves;
	} StructureBuffers;

	// Buffer textures so we can access the bvh data from the shader
	struct {
		TextureBuffer Nodes;
		TextureBuffer Leaves;
	} Samplers;
	
	//AABB CreateBoundingBox(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes);

	//std::vector<TriangleCentroid> SortAxis(const std::vector<TriangleCentroid>& Centroids, uint32_t Axis);
	
	//Split CreateSplit(const std::vector<TriangleCentroid>& SortedCentroids, const std::vector<AABB>& TriangleBoundingBoxes, size_t CentroidIdx);

	//Split FindBestSplit(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes, uint32_t Axis);
	//Split ChooseBestSplit(const Split& X, const Split& Y, const Split& Z);

	//void RefineSplit(Split& BestSplit, const std::vector<TriangleCentroid>& Centroids, const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices);

	//void DebugPrintBVH(const std::vector<NodeSerialized>& Nodes, const std::vector<int32_t>& LeafContents);
};
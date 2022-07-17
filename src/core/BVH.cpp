#include "BVH.h"
#include "../misc/TimeUtil.h"
#include <stack>
#include <algorithm>
#include <list>
#include <queue>
#include <thread>
#include <stdio.h>
#include <iostream>
#include <future>
#include <condition_variable>

void DebugPrintBVH(const std::vector<NodeSerialized>& Nodes, const std::vector<int32_t>& LeafContents);

using BVH = BoundingVolumeHierarchy;

// 4T optimal on FX 8350
constexpr size_t WorkerThreadCount = 4;

struct ConstructionNode {
	NodeUnserialized* DataPtr;
	int32_t Depth;
};

// Allocates nodes in chunks
struct NodeAllocator {
	constexpr static size_t PoolSize = 16384;

	std::vector<std::vector<NodeUnserialized>> MemoryPools;
	size_t PoolIndex;

	std::vector<NodeUnserialized>::iterator NextFreeNode;

	void AllocatePool(size_t PIdx) {
		MemoryPools.push_back(std::vector<NodeUnserialized>(PoolSize));
		PoolIndex = PIdx;

		NextFreeNode = MemoryPools[PIdx].begin();
	}

	void AllocNewPool(void) {
		PoolIndex++;
		AllocatePool(PoolIndex);
	}

	NodeAllocator(void) : PoolIndex(SIZE_MAX) {
		// Allocate initial pool
		AllocNewPool();
	}

	NodeUnserialized* AllocateNode(void) {
		// Inc iter, alloc pool if current pool has no free nodes remaining
		NextFreeNode++;
		if (NextFreeNode == MemoryPools[PoolIndex].end()) {
			AllocNewPool();
		}
		// Return address of iter
		return &*NextFreeNode;
	}
};

inline Triangle AssembleTriangle(const Vertex* Vertices, const TriangleIndexData& TriIdxInfo) {
	Triangle CurrentTriangle;

	CurrentTriangle.Vertices[0] = Vertices[TriIdxInfo.Indices[0]];
	CurrentTriangle.Vertices[1] = Vertices[TriIdxInfo.Indices[1]];
	CurrentTriangle.Vertices[2] = Vertices[TriIdxInfo.Indices[2]];

	return CurrentTriangle;
}

inline std::vector<TriangleCentroid> SortAxis(const std::vector<TriangleCentroid>& Centroids, uint32_t Axis) {

	std::vector<TriangleCentroid> Sorted(Centroids);

	std::sort(
		Sorted.begin(), Sorted.end(),
		[Axis](const TriangleCentroid& Arg0, const TriangleCentroid& Arg1) -> bool {
			return Arg0.Position[Axis] < Arg1.Position[Axis];
		}
	);

	return Sorted;
}

inline AABB CreateBoundingBox(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes) {
	// Loop through all centroids and get their triangle to create box
	AABB Box;

	for (const TriangleCentroid& Centroid : Centroids) {
		Box.Extend(TriangleBoundingBoxes[Centroid.Index]);
	}

	return Box;
}

// Precompute bounding boxes with another transfer split like algorithm
inline std::vector<AABB> PrecomputeBoundingBoxes(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes, size_t Count) {
	std::vector<AABB> BoundingBoxes;
	BoundingBoxes.reserve(Count);

	AABB CurrentExtent;

	for (size_t CentroidIndex = 0; CentroidIndex < Count; CentroidIndex++) {
		CurrentExtent.Extend(TriangleBoundingBoxes[Centroids[CentroidIndex].Index]);

		BoundingBoxes.push_back(CurrentExtent);
	}

	// Reverse since transfer algorithm iteratively adds, not subtractes like the loop
	std::reverse(BoundingBoxes.begin(), BoundingBoxes.end());

	return BoundingBoxes;
}

inline Split FindBestSplit(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes, uint32_t Axis, uint32_t PreviousAxis) {
	struct FastSplit {
		AABB BoundingBox[2];
		size_t CentroidCounts[2];
		float SAH;

		void Initialize(const std::vector<TriangleCentroid>& SortedCentroidArray) {
			CentroidCounts[0] = SortedCentroidArray.size();
			CentroidCounts[1] = 0;
		}

		const TriangleCentroid& Transfer(const std::vector<TriangleCentroid>& SortedCentroidArray) {
			size_t ReverseIndexIter = --CentroidCounts[0];
			                          CentroidCounts[1]++;

									  return SortedCentroidArray[ReverseIndexIter];
		}

		float ComputeSAH(void) {
			SAH = BoundingBox[0].SurfaceAreaHalf() * CentroidCounts[0] + BoundingBox[1].SurfaceAreaHalf() * CentroidCounts[1];
			return SAH;
		}
	};

	std::vector<TriangleCentroid> SortedCentroidArray = Axis == PreviousAxis ? Centroids : SortAxis(Centroids, Axis);
	size_t TransferableCentroids = SortedCentroidArray.size() - 1;
	std::vector<AABB> BoundingBoxes = PrecomputeBoundingBoxes(SortedCentroidArray, TriangleBoundingBoxes, TransferableCentroids);

	FastSplit CurrentSplit;
	CurrentSplit.Initialize(SortedCentroidArray);

	Split BestSplit;
	size_t BestSplitIndex = 0;

	for (size_t CentroidIndex = 0; CentroidIndex < TransferableCentroids; CentroidIndex++) {
		// Create bounding boxes and everything. 
		CurrentSplit.BoundingBox[0] = BoundingBoxes[CentroidIndex]; // This might benfit from a check of whether the centroid had an effect on the bounding box
		// Transfer the centroid
		CurrentSplit.BoundingBox[1].Extend(TriangleBoundingBoxes[CurrentSplit.Transfer(SortedCentroidArray).Index]);

		// Compute SAH and update if it's better than the current best split
		if (CurrentSplit.ComputeSAH() < BestSplit.SAH) {
			BestSplit.SAH = CurrentSplit.SAH;

			BestSplit.Box[0] = CurrentSplit.BoundingBox[0];
			BestSplit.Box[1] = CurrentSplit.BoundingBox[1];

			BestSplitIndex = CentroidIndex;
		}
	}
	BestSplitIndex++;

	BestSplit.Centroids[0] = std::move(std::vector<TriangleCentroid>(SortedCentroidArray.cbegin() , SortedCentroidArray.cend()    - BestSplitIndex));
	BestSplit.Centroids[1] = std::move(std::vector<TriangleCentroid>(SortedCentroidArray.crbegin(), SortedCentroidArray.crbegin() + BestSplitIndex));

	return BestSplit;
}

inline Split ChooseBestSplit(const Split& X, const Split& Y, const Split& Z, uint32_t& Axis) {
	if (Z.SAH > X.SAH&& Z.SAH > Y.SAH) {
		if (Y.SAH < X.SAH) {
			Axis = 1;
			return Y;
		}
		else {
			Axis = 0;
			return X;
		}
	}
	else {
		Axis = 3;
		return Z;
	}
}

NodeUnserialized::NodeUnserialized(void) : Children{ nullptr, nullptr }, Type(NodeType::NODE), parent(nullptr) {}

Split::Split(void) : SAH(FLT_MAX) {}

float Split::ComputeSAH(void) {
	// Fun fact: we don't need the actual surface area, just a quantity porportional to it. So we can avoid multiplying by 2 in this case
	SAH = Box[0].SurfaceAreaHalf() * Centroids[0].size() + Box[1].SurfaceAreaHalf() * Centroids[1].size();
	return SAH;
}

float NodeUnserialized::ComputeSAH(void) {
	return BoundingBox.SurfaceAreaHalf() * Centroids.size();
}

// Terminate if all threads are waiting
bool AllThreadWaitingConditions(bool* WaitConditions) {
	bool AllWait = true;

	for (size_t Counter = 0; Counter < WorkerThreadCount; Counter++) {
		AllWait &= WaitConditions[Counter];
	}

	return AllWait;
};

void MakeLeaf(ConstructionNode& CurrentNode, std::vector<int32_t>& LBuf, std::mutex& LBufMutex) {
	CurrentNode.DataPtr->Type = NodeType::LEAF;

	std::lock_guard<std::mutex> LBufLock(LBufMutex);

	CurrentNode.DataPtr->Leaf.Offset = (int32_t)LBuf.size();
	CurrentNode.DataPtr->Leaf.Size   = (int32_t)CurrentNode.DataPtr->Centroids.size();

	for (const TriangleCentroid& Tri : CurrentNode.DataPtr->Centroids) {
		LBuf.push_back(Tri.Index);
	}
}

void ParallelConstructionTask(
	std::condition_variable& WorkSignal,
	std::mutex& WorkMtx,
	bool* WaitConditions,
	bool& ThreadWait,
	const std::vector<AABB>& TriAABBs,
	std::stack<ConstructionNode>& ConstructionNodes,
	std::mutex& ConstructionMutex,
	std::vector<int32_t>& LBuf,
	std::mutex& LBufMutex,
	NodeAllocator& Alloc,
	std::mutex& AllocMutex
) {
	while (true) {
		ConstructionNode CurrentNode;

		bool FirstWait = true;
		while (true) {
			ConstructionMutex.lock();
			if (!ConstructionNodes.empty()) {
				CurrentNode = ConstructionNodes.top();
				ConstructionNodes.pop();

				ConstructionMutex.unlock();
				break;
			} else {
				ConstructionMutex.unlock();

				if (FirstWait) {
					ThreadWait = true;
					FirstWait = false;

					WorkSignal.notify_all();
				}

				if (AllThreadWaitingConditions(WaitConditions)) {
					return;
				}

				std::unique_lock<std::mutex> SignalLock(WorkMtx);
				WorkSignal.wait(SignalLock); // Wait until next update

			}
		}

		ThreadWait = false;

		// If this node has just 1 triangle, let's just turn it into a leaf immediatly without any try-spliting 
		if (CurrentNode.DataPtr->Centroids.size() < 2) {
			MakeLeaf(CurrentNode, LBuf, LBufMutex);
			continue;
		}

		// Next try to find the best split on all 3 axes
		Split TentativeSplits[3];

		TentativeSplits[0] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriAABBs, 0, CurrentNode.DataPtr->SplitAxis);
		TentativeSplits[1] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriAABBs, 1, CurrentNode.DataPtr->SplitAxis);
		TentativeSplits[2] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriAABBs, 2, CurrentNode.DataPtr->SplitAxis);
		// Find/selelct the best split from all axes 
		uint32_t ChosenAxis;
		Split BestSplit = ChooseBestSplit (
			TentativeSplits[0],
			TentativeSplits[1],
			TentativeSplits[2],
			ChosenAxis
		);

		// Subdivision termination taken from Jacco Bikker, "The Perfect BVH", slide #12
		if (CurrentNode.DataPtr->Centroids.size() <= MAX_LEAF_TRIANGLES && BestSplit.SAH > CurrentNode.DataPtr->ComputeSAH()) {
			MakeLeaf(CurrentNode, LBuf, LBufMutex);
		} else {
			// Construct children nodes
			ConstructionNode Children[2];

			AllocMutex.lock();
			Children[0].DataPtr = Alloc.AllocateNode();
			Children[1].DataPtr = Alloc.AllocateNode();
			AllocMutex.unlock();

			Children[0].DataPtr->BoundingBox = BestSplit.Box[0];
			Children[1].DataPtr->BoundingBox = BestSplit.Box[1];

			Children[0].DataPtr->Centroids = BestSplit.Centroids[0];
			Children[1].DataPtr->Centroids = BestSplit.Centroids[1];

			Children[0].DataPtr->SplitAxis = ChosenAxis;
			Children[1].DataPtr->SplitAxis = ChosenAxis;

			int32_t ChildDepth = CurrentNode.Depth + 1;

			Children[0].Depth = ChildDepth;
			Children[1].Depth = ChildDepth;

			CurrentNode.DataPtr->Children[0] = Children[0].DataPtr;
			CurrentNode.DataPtr->Children[1] = Children[1].DataPtr;

			// Push nodes onto the stack
			ConstructionMutex.lock();
			ConstructionNodes.push(Children[0]);
			ConstructionNodes.push(Children[1]);
			ConstructionMutex.unlock();
			
			WorkSignal.notify_all();
		}
	}
}

// Prevent construction bug where all threads sleep
void RenotifyThreads(std::condition_variable& WorkSignal, bool& WorkerThreadsRunning) {
	while (WorkerThreadsRunning) {
		WorkSignal.notify_all();
		// Renotify every 1 ms
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	//std::cout << "Done renotifying\n";
}

void BoundingVolumeHierarchy::BuildFullSweep(std::vector<CompactTriangle>& triangles) {
	//std::cout << "Start of BVH construction" << std::endl;

	Timer ConstructionTimer;

	// First things first. We need to build a list of triangles

	ConstructionTimer.Begin();

	std::vector<TriangleCentroid> CentroidList;
	CentroidList.reserve(triangles.size());

	std::vector<AABB> TriangleBoundingBoxes;
	TriangleBoundingBoxes.reserve(triangles.size());

	for (uint32_t i = 0; i < triangles.size(); i++) {
		CompactTriangle currentTriangle = triangles[i];

		AABB Box;
		Box.Extend(currentTriangle.position0);
		Box.Extend(currentTriangle.position1);
		Box.Extend(currentTriangle.position2);

		TriangleCentroid Centroid;
		Centroid.Index = i;
		Centroid.Position = Box.Center();

		CentroidList.push_back(Centroid);
		TriangleBoundingBoxes.push_back(Box);
	}

	ConstructionTimer.End();
	//ConstructionTimer.DebugTime();
	ConstructionTimer.Begin();
	
	NodeAllocator Allocator;
	std::mutex AllocatorMutex;

	std::stack<ConstructionNode> ConstructionNodeStack;
	std::mutex ConstructionStackMutex;

	std::vector<int32_t> LeafContentBuffer;
	std::mutex LeafContentMutex;

	NodeUnserialized* RootNode = Allocator.AllocateNode();

	RootNode->BoundingBox = CreateBoundingBox(CentroidList, TriangleBoundingBoxes);
	RootNode->Centroids   =                   CentroidList;
	RootNode->SplitAxis   = UINT32_MAX;

	ConstructionNode CRN; // CRN = construction root node

	CRN.DataPtr = RootNode;
	CRN.Depth = 0;

	ConstructionNodeStack.push(CRN);

	bool ThreadWaitingConditions[WorkerThreadCount]{ false };

	std::future<void> WorkerThreads[WorkerThreadCount];

	std::condition_variable WorkSignal;
	std::mutex WorkMtx;

	// First begin the renotification thread
	bool WorkerThreadsRunning = true;
	std::future<void> RenotificationThread = std::async(std::launch::async, RenotifyThreads, std::ref(WorkSignal), std::ref(WorkerThreadsRunning));

	for (size_t Index = 0; Index < WorkerThreadCount; Index++) {
		WorkerThreads[Index] = std::async(
			std::launch::async,
			ParallelConstructionTask,
			std::ref(WorkSignal),
			std::ref(WorkMtx),
			ThreadWaitingConditions, 
			std::ref(ThreadWaitingConditions[Index]),
			std::ref(TriangleBoundingBoxes),
			std::ref(ConstructionNodeStack),
			std::ref(ConstructionStackMutex),
			std::ref(LeafContentBuffer),
			std::ref(LeafContentMutex),
			std::ref(Allocator),
			std::ref(AllocatorMutex)
		);
	}

	for (size_t Index = 0; Index < WorkerThreadCount; Index++) {
		WorkerThreads[Index].wait();
	}

	WorkerThreadsRunning = false;
	// Wait just to make sure it's done
	RenotificationThread.wait();

	ConstructionTimer.End();
	//ConstructionTimer.DebugTime();
	ConstructionTimer.Begin();

	std::queue<NodeUnserialized*> IndexConnectionQueue;
	IndexConnectionQueue.push(RootNode);

	// This vector will contain our nodes that we have processed
	std::vector<NodeSerialized> ProcessedNodes;

	// This while loop connects indices
	while (!IndexConnectionQueue.empty()) {
		NodeUnserialized* CurrentNode = IndexConnectionQueue.front();
		IndexConnectionQueue.pop();

		NodeSerialized SerializedNode;

		SerializedNode.BoundingBox = CurrentNode->BoundingBox;
		SerializedNode.parent = 0;

		if (CurrentNode->Type == NodeType::NODE) {
			SerializedNode.firstChild = ProcessedNodes.size() + IndexConnectionQueue.size() + 1;

			vec3 seperation = (CurrentNode->Children[1]->BoundingBox.Center() - CurrentNode->Children[0]->BoundingBox.Center());
			vec3 magnitude = abs(seperation);

			int axis;
			if (magnitude.x > magnitude.y&& magnitude.x > magnitude.z) {
				axis = 0;
			}
			else if (magnitude.y > magnitude.x&& magnitude.y > magnitude.z) {
				axis = 1;
			}
			else {
				axis = 2;
			}

			// Assuming that the first node is closer to positive, the other one closer to the negative (normal traversal seems to prefer it)
			if (seperation[axis] < 0) {
				std::swap(CurrentNode->Children[0], CurrentNode->Children[1]);
			}

			// axis takes up at most 2 bits
			SerializedNode.parent |= (axis << 30);

			IndexConnectionQueue.push(CurrentNode->Children[0]);
			IndexConnectionQueue.push(CurrentNode->Children[1]);
		} else {
			SerializedNode.triangleRange = (CurrentNode->Leaf.Size & 15) | (CurrentNode->Leaf.Offset << 4);
			SerializedNode.MakeLeaf();

			int range = -SerializedNode.triangleRange;
			if ((range >> 4) != CurrentNode->Leaf.Offset || (range & 15) != CurrentNode->Leaf.Size) {
				exit(-1);
			}
		}

		if (CurrentNode->parent) {
			SerializedNode.parent |= CurrentNode->parent->Index;
		}

		ProcessedNodes.push_back(SerializedNode);
	}

	for (int i = 0; i < 16; i++) {
		std::cout << ProcessedNodes[i].firstChild  << '\t' << ProcessedNodes[ProcessedNodes[i].firstChild].firstChild << '\n';
	}

	ConstructionTimer.End();
	//ConstructionTimer.DebugTime();
	ConstructionTimer.Begin();

	nodesVec = ProcessedNodes;

	// This is very unsafe and super bad according to some C++ programmers but we live on the edge and we want the edge of performance
	for (NodeSerialized& node : ProcessedNodes) {
		struct NewLayout {
			vec3 min;
			int data0;
			vec3 max;
			int data1;
		} temp;

		temp.min = node.BoundingBox.min;
		temp.data0 = node.firstChild;
		temp.max = node.BoundingBox.max;
		temp.data1 = node.parent;

		NewLayout* ptr = (NewLayout*)&node;
		*ptr = temp;
	}

	nodesBuf.CreateBinding(BUFFER_TARGET_ARRAY);
	nodesBuf.UploadData(ProcessedNodes, GL_STATIC_DRAW);

	nodesTex.CreateBinding();
	nodesTex.SelectBuffer(&nodesBuf, GL_RGBA32F);

	referenceBuf.CreateBinding(BUFFER_TARGET_ARRAY);
	referenceBuf.UploadData(LeafContentBuffer, GL_STATIC_DRAW);

	referenceTex.CreateBinding();
	referenceTex.SelectBuffer(&referenceBuf, GL_R32F);

	ConstructionTimer.End();
	ConstructionTimer.DebugTime();

	//DebugPrintBVH(ProcessedNodes, LeafContentBuffer);

	//std::cout << "End of BVH construction" << std::endl;
}

/*
1       5
5       9
3       13
13      17
11      21
9       25
7       29
29      33
27      37
25      41
23      45
21      49
19      53
17      57
15      61
61      65

1       3
3       7
5       11
7       15
9       19
11      23
13      27
15      31
17      35
19      39
21      43
23      47
25      51
27      55
29      59
31      63
*/


void NodeSerialized::MakeLeaf(void) {
	triangleRange = -triangleRange;
}

NodeType NodeSerialized::GetType(void) {
	return triangleRange < 0 ? NodeType::LEAF : NodeType::NODE;
}

bool NodeSerialized::Intersect(const Ray& ray, HitInfo& hit, const std::vector<CompactTriangle>& triangles) {
	int i = triangleRange;
	int j = i - triangleRange;

	bool result = false;
	for (int k = i; k < j; k++) {
		auto triangle = triangles[k];
		result |= triangle.Intersect(ray, hit);
	}

	return result;
}

void IndentDebugBVH(int32_t TabCount) {
	for (int32_t Counter = 0; Counter < TabCount; Counter++) {
		printf("\t");
	}
}

/*

void DebugPrintBVH(const std::vector<NodeSerialized>& Nodes, const std::vector<int32_t>& LeafContents) {
	struct NodeStackPrintItem {
		NodeSerialized Node;
		int32_t Depth;
	};

	NodeStackPrintItem RootNode;

	RootNode.Node = Nodes[0];
	RootNode.Depth = 0;

	int32_t CurrentDepth;

	std::stack<NodeStackPrintItem> NodeStack;
	NodeStack.push(RootNode);

	while (!NodeStack.empty()) {
		NodeStackPrintItem CurrentItem = NodeStack.top();
		NodeStack.pop();

		CurrentDepth = CurrentItem.Depth;

		int32_t NumTabs = CurrentDepth;

		IndentDebugBVH(NumTabs);

		if (CurrentItem.Node.GetType() == NodeType::NODE) {
			printf("Node: Depth %i\n", CurrentDepth);

			CurrentDepth++;

			NodeStackPrintItem ChildItems[2];

			ChildItems[0].Node = Nodes[CurrentItem.Node.firstChild[0]];
			ChildItems[0].Depth = CurrentDepth;

			ChildItems[1].Node = Nodes[CurrentItem.Node.firstChild[1]];
			ChildItems[1].Depth = CurrentDepth;

			NodeStack.push(ChildItems[0]);
			NodeStack.push(ChildItems[1]);
		}
		else {
			printf("Leaf: Offset %i\tSize %i\n", CurrentItem.Node.triangleRange.Offset + 1, -CurrentItem.Node.triangleRange.Size);

			IndentDebugBVH(NumTabs);

			printf("Contents: ");

			for (int32_t TriCounter = CurrentItem.Node.triangleRange.Offset; TriCounter < CurrentItem.Node.triangleRange.Offset - CurrentItem.Node.triangleRange.Size; TriCounter++) {
				printf("%i ", LeafContents[TriCounter]);
			}

			printf("\n");
		}

		IndentDebugBVH(NumTabs);

		printf("Bounds: (%f %f %f) -> (%f %f %f)\n",
			CurrentItem.Node.BoundingBox.Min.x, CurrentItem.Node.BoundingBox.Min.y, CurrentItem.Node.BoundingBox.Min.z,
			CurrentItem.Node.BoundingBox.Max.x, CurrentItem.Node.BoundingBox.Max.y, CurrentItem.Node.BoundingBox.Max.z
		);
	}
}
*/

// Use negative indices to differentiate between leaves and nodes. We use size here since size shouldn't be 0 but offset could be 0.
	// Decrement to fix weird triangle front face culling bug
// Leaf.Offset--;

/*
void IndentDebugBVH(int32_t TabCount) {
	for (int32_t Counter = 0; Counter < TabCount; Counter++) {
		printf("\t");
	}
}
	//exit(0);

void BoundingVolumeHierarchy::DebugPrintBVH(const std::vector<NodeSerialized>& Nodes, const std::vector<int32_t>& LeafContents) {
	struct NodeStackPrintItem {
		NodeSerialized Node;
		int32_t Depth;
	};

	NodeStackPrintItem RootNode;

	RootNode.Node = Nodes[0];
	RootNode.Depth = 0;

	int32_t CurrentDepth;

	std::stack<NodeStackPrintItem> NodeStack;
	NodeStack.push(RootNode);

	while (!NodeStack.empty()) {
		NodeStackPrintItem CurrentItem = NodeStack.top();
		NodeStack.pop();

		CurrentDepth = CurrentItem.Depth;

		int32_t NumTabs = CurrentDepth;

		IndentDebugBVH(NumTabs);

		if (CurrentItem.Node.GetType() == NodeType::NODE) {
			printf("Node: Depth %i\n", CurrentDepth);

			CurrentDepth++;

			NodeStackPrintItem ChildItems[2];

			ChildItems[0].Node = Nodes[CurrentItem.Node.ChildrenNodes[0]];
			ChildItems[0].Depth = CurrentDepth;

			ChildItems[1].Node = Nodes[CurrentItem.Node.ChildrenNodes[1]];
			ChildItems[1].Depth = CurrentDepth;

			NodeStack.push(ChildItems[0]);
			NodeStack.push(ChildItems[1]);
		} else {
			printf("Leaf: Offset %i\tSize %i\n", CurrentItem.Node.Leaf.Offset + 1, -CurrentItem.Node.Leaf.Size);

			IndentDebugBVH(NumTabs);

			printf("Contents: ");

			for (int32_t TriCounter = CurrentItem.Node.Leaf.Offset; TriCounter < CurrentItem.Node.Leaf.Offset - CurrentItem.Node.Leaf.Size; TriCounter++) {
				printf("%i ", LeafContents[TriCounter]);
			}

			printf("\n");
		}

		IndentDebugBVH(NumTabs);

		printf("Bounds: (%f %f %f) -> (%f %f %f)\n",
			CurrentItem.Node.BoundingBox.Min.x, CurrentItem.Node.BoundingBox.Min.y, CurrentItem.Node.BoundingBox.Min.z,
			CurrentItem.Node.BoundingBox.Max.x, CurrentItem.Node.BoundingBox.Max.y, CurrentItem.Node.BoundingBox.Max.z
		);
	}
}*/

/*

Proposed split algorithm: The transfer split

We carry a single centroid from one side to another 

Technical details:

First initialize one side of the split (in our case 0) with all the centroids 
Compute SAH and update the best split with it if it is the best SAH
Then transfer a single centroid to the other side (in our case 1)
Extend 1's bounds and recalculate 0's bounds
I'm not sure if storing the bound values beforehand helps but its's probably
going to make memory consumption really bad

That advantage of the split compared to the other one is that we don't 
start again each time, which means copying complexity goes from

N^2-N (derived from N(N-1)

to

N-1

#define EARLY_SPLIT_TERMINATION

#ifdef EARLY_SPLIT_TERMINATION
	float LastSplitScore = 1.0e20f;
#endif

	// Splitting algorithm created by madmann
	for (size_t CentroidIdx = 1; CentroidIdx < SortedCentroids.size(); CentroidIdx++) {
		Split CurrentSplit = CreateSplit(SortedCentroids, Vertices, Indices, TriangleBoundingBoxes, Axis, CentroidIdx);

		float CurrentSplitScore = CurrentSplit.ComputeSAH();

// Turn this on for testing with really high res models for faster BVH construction
#ifdef EARLY_SPLIT_TERMINATION

		//Derivative/finite difference is now increasing, and it is likely that future splits will be even worse
		//We can terminate the search for the best score early

		if (CurrentSplitScore > LastSplitScore) {
			// Early termination speeds up the BVH building process with not significant impact on tree quality
			break;
		}
#endif
		// I could put this into an else for the early termination if
		if (CurrentSplitScore < BestSplit.SAH) {
			BestSplit = CurrentSplit;
		}

#ifdef EARLY_SPLIT_TERMINATION
		LastSplitScore = CurrentSplitScore;
#endif
	}

	BestSplit.Axis = Axis;

	return BestSplit;


#if 0
	std::future<void> Copier0 = std::async(std::launch::async, CopyFunc0, &CurrentSplit);
	std::future<void> Copier1 = std::async(std::launch::async, CopyFunc1, &CurrentSplit);

	Copier0.wait();
	Copier1.wait();
#else
	CopyFunc0(&CurrentSplit);
	CopyFunc1(&CurrentSplit);
#endif


	auto CopyFunc0 = [SortedCentroids, CentroidIdx](Split* OutputSplit) -> void {
		std::copy_n(
			SortedCentroids.begin(),
			CentroidIdx,
			std::back_inserter(OutputSplit->Centroids[0])
		);
	};

	auto CopyFunc1 = [SortedCentroids, CentroidIdx](Split* OutputSplit) -> void {
		std::copy(
			SortedCentroids.begin() + CentroidIdx,
			SortedCentroids.end(),
			std::back_inserter(OutputSplit->Centroids[1])
		);
	};
*/


/*
First we need to construct the root node and push it onto the stack
The root node is nothing to complicated
It's literally just the same as computing a bounding box
*/

// Memory pool for nodes. Ideally I would use an "index" based pool


/*
TLDR for the BVH construction algorithm:
We create a split between all midpoints for centroids of the triangle for all 3 axes
We then rate each split using the SAH and choose the one with the lowest SAH
Then we serealize it
*/

// TODO: pre-sorting
	// std::vector<TriangleCentroid> SortedCentroidList[3];

	// SortedCentroidList[0] = SortAxis(CentroidList, 0);
	// SortedCentroidList[1] = SortAxis(CentroidList, 1);
	// SortedCentroidList[2] = SortAxis(CentroidList, 2);

	//struct SortedCentroidReference {

	//};

	// After building the list of triangles, we need to build the BVH

	

	/*
	TODO: get a better algorithm for this

	Brute force approach: we split the nodes at all possible midpoints between centroids
	We do this in all 3 axises or what ever the plural form of that word is
	We use a rating system (SAH in our case) to rate the BVH split and keep track
	of the split with the best SAH score. Once we are done splitting we use the best
	split to continue BVH construction

	Implementation details:
	We only follow this procedure if the number of remaining triangles is too large to efficiently traverse as a leaf
	We first sort the centroids by distance from the positive direction of each axis for each axis

	Then for each consequtive centroids, we calculate a midpoint between them
	This midpoint gets used as the center of the hyperplane that splits the node into 2 children
	Based on this split we tenatively create the child nodes

	This leaves us with another question: what do we use to rate the split?
	Thankfully, there exists a powerful rating system known as the surface area heuristic, short for SAH.
	SAH in an nutshell uses surface area of a node to compute how effective a split is. We use the SAH equation to rate the split.
	If the SAH score is lower than the previous score, we keep track of the current split and overwrite the previous one
	SAH equation from https://www.youtube.com/watch?v=57AcnwW4pxw&ab_channel=ComputerGraphicsatTUWien at 36:02

	Equation 1
	SAH score for child N = surface area of child N's bounding box * number of triangles in child N
	SAH score for split   = SAH score for child 1 + SAH score for child 2

	For the SAH score, lower is better. (1) can also easily be applied to wide BVHs:

	Equation 2
	SAH score for split   = Sum from 0 to N using counter n: SAH score for child n

	After we have processed all splits we use the best SAH scoring split

	Stack based constructiomn
	Based on how GPUs traverse the BVH, we can do the same to make a non recursive method of creating a BVH

	We first create a root node for the BVH
	Then we push it on to the stack

	Then in a while loop we continously split the BVH and push nodes onto the stack until it's empty

	Additionally what I could do with my splitting algorithm is to do binary refinement after I find the best split
	My algorithm considers midpoints in order to discretize the continous space of the bounding volume
	However this might be a very inaccurate discretization in certain cases with large or few triangles (or even both)
	To further increase efficenciy of the split I can do a binary refinement of the best choosen split
	I can extend this to best chosen split of all 3 axes which is quite benefitial in some cases
	This helps ust in case the discretization made the algorithm choose the wrong axis

	Overview of the algorithm in terms of performance and memory consumption

	This algorithm is very very bad, despite being simple. Computer graphics is an area of programming where doing
	really, really complex stuff pays off a lot (example: manifold path tracing). BVHs are not different at all
	There's like 1000 new papers coming out each day about better BVH construction and traversal. The downsides of my
	algorithm really begin to show with high res/poly count models. We store the triangle centroid 3 times becuase we
	need it for each of the 3 axes. Storing the centroid is is 3 + 1 4 byte elements (3 for the position, 1 for the
	index to the triangle index buffer for the centroid's parent triange. Basically an index to set of indices)
	That means we store centroids on a 16 byte basis. Now, let's assume we are creating the initial split for a 70k
	triangle mesh. 70,000 * 3 * 16. That's 3,360,000 bytes, 3,281 KB, or 3.2 MB, whichever unit you prefer to use
	(I prefer MB here, so I will use it for any further data measurements). Overall, we are storing 48 bytes per triangle
	The bad thing here is that the standard for (high quality) rendering is getting higher and higher and we now commonly
	seeing upwards of 2 million per mesh, which would require 91.6 MB. Now let's talk perforamnce. For an N number of
	triangles, there are N - 1 midpoints on each axis, so we can use the forumla 3N - 3 (derived from distributin 3
	in 3(N - 1)) to find total number of midpoints. That's a lot of splits we have to check. Now for each split, we have to
	sort each triangle as "in front of the hyper plane" or "behind the hypler plane". So for each axis we have (N-1) * O(N)
	efficency. We also have to test for efficency of the surface area algorithm. See AABB.cpp for surface area calculations,
	but I will omit it from efficency calculations for both simplicty's sake and the fact that it doesn't cost must given that
	you are a smart programmer and mathematician. I'n my impl I will be using std::sort, which has an efficency of N*log2(N)
	So in total, our algorithm runs at a complexity of 3((N-1)*N*log2(N)) which simplyfies to 3N^2log2(N) - 3Nlog2(N). Plotting
	this on desmos makes our algorithm look bad to say the least (f\left(N\right)=3N^{2}\log_{2}\left(N\right)-3N\log_{2}\left(N\right))
	For a 70k model, we have a complexity of 236.6 million. For a 2 million triangle model, we have 251.2 billion complexity.
	The number of splits for a 2M model is 5,999,997 so the bottle neck here is the sorting algorithm. Now remember, the statistics
	shown here are only for the root node. Although decreasing exponentially, you could see a complexity of ~500 billion for the 2M
	model for a BVH with a tree depth of 18, which amounts to 7-8 triangles per leaf. With such high tree depth, we will have to consider
	memory usage of the BVH, but that's for alater day. One more thing to node is that with such a brute force strategy,
	we cannot easily parellelize it well and cannot adapt it efficently to dynamic scenes or animated meshes

	Now implementation specifics:

	I do my calculations in a 2-pass process
	First I construct the BVH with all of it's unprocessed data like pointers and vectors
	Then I convert it into something that can be serealized and send off to the GPU

	Each unserealized node contains an AABB to represent it. I also have a list of triangles within the node
	There are also pointers to the children, which will be initialized to nullptr. We also will have a type
	enum to seperate nodes from leaves.

	Let's get the basic stuff out of the way first: leaf building
	We begin by first checking whether the number of triangles within
	the current node is small enough for it to be converted into a leaf.
	If it is small enough, we make appends to a vector containing the leaf
	contents. First of all, we first set the serealized leaf pointer's
	offset value to that of the leaf content vector size. The I set size
	to the size of the centroid list within that node. And then I copy
	all of the index contents of the centroid vector to the leaf content
	vector.

	So we have another question: How do we determine whether a node is a
	good canidate to be a leaf? We simply check the number of triangles
	in that leaf. In my case, I will use 8 triangles per leaf, although
	there are probably better algorithms out there for this sort of thing
	I could use a SAH type thing where I do number of triangles * surface
	area of AABB but that makes things to complicated and I would have to
	add checks to prevent infinite recursion with large triangles

	One thing to node is that I will add unsearlized nodes to a linked list
	instead of a vector since I will likely not be iterating over them most
	of the time and instead will be adding them. I also need support for
	portions of the list pointing to another pair of elements in the list
	and vectors can move the memory around which is not what I want in this
	case.

	Now let's talk about serealization. Ideally, to make things somewhat
	cache friendly since I will probably miss a lot of nodes and will be
	iterating over them most of the time, I will make nodes with the same
	depths consective. To do this simply, I need to add an interger
	variable to the unserealized node structure. This will be it's serealized
	index within the node texture. I will first begin by pushing the root node
	into a queue. One thing to note is that I first put everything into a
	vector of pointers to the nodes before filling a serealized vector.
	I will use a while loop to do the serealizaation process. But before
	entering the while loop, I create an interger variable counter that
	represents the index for the texture. So remember how I pushed
	the root node into a queue? Now once we enter the queue, we pop it.
	We set it's index, and then push all of it's non leaf children on
	to a queue. Maybe here I could sort them by size as a minor optimization
	but whatever. I continue the seralization while loop. For each node, I set
	its index and then push it to a queue if it is not a leaf

	Once I have set it's index, I do a second loop with a queue. This time,
	I create a serealized node and send it off into a vector which will
	be sent to the GPU. For each node that gets popped off the queue, I copy
	basic information like the AABB. Then, if it is not a leaf, I will go
	through it's children and copy their indices to the current node
	before pushing those children onto the queue as well. If the node is a
	leaf, I will instead copy it's index information and then make it a
	leaf by negating the number of triangles in the leaf to mark it as a
	leaf. Once I am done processing the node or leaf, I send it into a vector
	One I am done doing all of that, I can send the vector to the GPU and
	start traversing it

	So let's start of with a basic definition of a unseralized node
	An unserealized node needs some stuff. This includes the number of triangles
	withing the node, an enumartion that differentiates it as a leaf or node,
	pointers to its children, and a serealized index
	See BVH.h for further details and the implementation

	Next we need to find a way to keep tract of a split. This isn't too difficult
	we just need to keep track of an axis and the distance from the min side to the max

	*/

	/*
	However nodes that need to be processed need different information.
	This information is stuff like the triangles within the
	*/

	/*
	First, build root node
	Second, allocate 2 child nodes and set up refrences so you don't have to deal with them
	Third, partition triangles to each node
	Fourth, process each node as in step 2
	*/

/*
	//DebugPrintBVH(ProcessedNodes, LeafContentBuffer);

	std::cout << LeafContentBuffer.size() << ' ' << Indices.size() << '\n';

	//for (const int32_t Value : LeafContentBuffer) {
	//	std::cout << Value << '\n';
	//}
*/

/*
void BoundingVolumeHierarchy::RefineSplit(Split& BestSplit, const std::vector<TriangleCentroid>& Centroids, const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices) {
	// We know that the centroid list is sorted so we can take the end of one and the beginning of another to find the midpoint
	float SplitPoint = BestSplit.Centroids[0].at(BestSplit.Centroids->size()).Position[BestSplit.Axis] + BestSplit.Centroids[1][0].Position[BestSplit.Axis];
	SplitPoint *= 0.5f;

	// We calculate the split derivatives/gradient and head off in that direction for a finite number of refinements

	// How far we step gets progressively smaller
	float RefinementSize = 0.5f;

	constexpr uint32_t MaxRefinements = 4;
	for (uint32_t RefinementCount = 0; RefinementCount < MaxRefinements; RefinementCount++) {
		// first index is if we move back, the second one is if we move forwards
		Split RefinedSplits[2];



		// Multiply by 0.5 so it is binary refinement
		RefinementSize *= 0.5f;
	}
}
*/

// There's probably a more fast way to do this

	/*
	if (X.SAH > Y.SAH && X.SAH > Z.SAH) {
		return X;
	} else if (Y.SAH > X.SAH&& Y.SAH > Z.SAH) {
		return Y;
	} else {
		return Z;
	}
	*/

/*

Split BoundingVolumeHierarchy::CreateSplit(const std::vector<TriangleCentroid>& SortedCentroids, const std::vector<AABB>& TriangleBoundingBoxes, size_t CentroidIdx) {
	Split CurrentSplit;

	CurrentSplit.Centroids[0].reserve(                         CentroidIdx);
	CurrentSplit.Centroids[1].reserve(SortedCentroids.size() - CentroidIdx);

	std::copy_n(
		SortedCentroids.begin(),
		CentroidIdx,
		std::back_inserter(CurrentSplit.Centroids[0])
	);

	std::copy(
		SortedCentroids.begin() + CentroidIdx,
		SortedCentroids.end(),
		std::back_inserter(CurrentSplit.Centroids[1])
	);

	CurrentSplit.Box[0] = CreateBoundingBox(CurrentSplit.Centroids[0], TriangleBoundingBoxes);
	CurrentSplit.Box[1] = CreateBoundingBox(CurrentSplit.Centroids[1], TriangleBoundingBoxes);

	return CurrentSplit;
}
*/

/*
Parellization:

Each worker thread waits for an avaiable node to take off the stack to work on
If there is not node to work on, we wait for one to be pushed onto the stack or terminate 
if the construction is done

How to signal construction is done:
If all worker threads are waiting for a node to subdivide, then that means there are no more
nodes to subdivide. Everything has been converted to a leaf. Basic pesudeocode for algorithm

Launcher thread func:
	push root node onto stack

	launch child threads

	wati for child threads to finish

Child thread func:
	while true:
		if stack is empty
			// Easily done by busy wait, and on multicore systems this isn't a problem
			// However this is still really bad practice
			wait until node is available or until all threads are waiting

		process node and push children nodes to stack

Implementation details:

Busy thread wait:

func bool locked_empty():
	lock stack
	bool empty = stack.empty()
	unlock stack
	return empty

func bool all_other_threads_are_waiting():
	bool all_waiting = OR waiting condition of all other threads
	return all_waiting

while true:
	if(locked_empty())
		waiting = true
		if all_other_threads_are_waiting():
			return
	else
		break
		
waiting = false

lock stack
get next node on stack
unlock stack

process node

lock stack
push children on to stack
unlock stack


	


*/

/*
while (!ConstructionNodeStack.empty()) {
	// Fetch the next node from the stack
	ConstructionNode CurrentNode = ConstructionNodeStack.top();
	ConstructionNodeStack.pop();

	// Check if node can be a leaf
	if (CurrentNode.DataPtr->Centroids.size() < LeafHint || (uint32_t)CurrentNode.Depth > DepthHint) {
		CurrentNode.DataPtr->Type = NodeType::LEAF;

		CurrentNode.DataPtr->Leaf.Offset = (int32_t)LeafContentBuffer.size()             ;
		CurrentNode.DataPtr->Leaf.Size   = (int32_t)CurrentNode.DataPtr->Centroids.size();

		for (const TriangleCentroid& Tri : CurrentNode.DataPtr->Centroids) {
			LeafContentBuffer.push_back(Tri.Index);
		}
	} else {
		// Next try to find the best split on all 3 axes
		Split TentativeSplits[3];

		TentativeSplits[0] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriangleBoundingBoxes, 0);
		TentativeSplits[1] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriangleBoundingBoxes, 1);
		TentativeSplits[2] = FindBestSplit(CurrentNode.DataPtr->Centroids, TriangleBoundingBoxes, 2);
		// Find/selelct the best split from all axes
		Split BestSplit = ChooseBestSplit(
			TentativeSplits[0],
			TentativeSplits[1],
			TentativeSplits[2]
		);
		// Construct children nodes
		ConstructionNode Children[2];

		Children[0].DataPtr = Allocator.AllocateNode();
		Children[1].DataPtr = Allocator.AllocateNode();

		Children[0].DataPtr->BoundingBox = BestSplit.Box[0];
		Children[1].DataPtr->BoundingBox = BestSplit.Box[1];

		Children[0].DataPtr->Centroids = BestSplit.Centroids[0];
		Children[1].DataPtr->Centroids = BestSplit.Centroids[1];

		CurrentNode.DataPtr->Children[0] = Children[0].DataPtr;
		CurrentNode.DataPtr->Children[1] = Children[1].DataPtr;

		int32_t ChildDepth = CurrentNode.Depth + 1;

		Children[0].Depth = ChildDepth;
		Children[1].Depth = ChildDepth;

		// Push nodes onto the stack
		ConstructionNodeStack.push(Children[0]);
		ConstructionNodeStack.push(Children[1]);
	}
}
*/

/*
Ideally, we want a node to be take if the stack isn't empty
If it is empty we wait for there to be a node or for all threads to 
exit. Here's how we handle node conditions:

Node CurrentNode

bool WaitForNextNode = true

while WaitForNextNode:
	Mutex.lock
	if !Stack.empty
		CurrentNode = Stack.NextNode
		WaitForNextNode = false
	Mutex.unlock
	if All other threads are waiting:
		return


*/


/*
void ParallelConstructionTask(
bool* WaitConditions,
bool* ThreadWait,
uint32_t LeafHint,
uint32_t DepthHint,
const std::vector<AABB>& TriAABBs,
std::stack<ConstructionNode>& ConstructionNodes,
std::mutex& ConstructionMutex,
std::vector<int32_t>& LBuf,
std::mutex& LBufMutex,
NodeAllocator& Alloc,
std::mutex& AllocMutex
)
	*/

/*
template <typename T>
struct ThreadSafe {
	T Item;
	std::mutex Mutex;

	// operator std::mutex& (void) {
	//	return Mutex;
	//}

	void Lock(void) {
		Mutex.lock();
	}

	void Unlock(void) {
		Mutex.unlock();
	}
};
*/

/*

bool LockedEmpty (const std::stack<ConstructionNode>& Nodes, std::mutex& Lock) {
	//std::lock_guard<std::mutex> EmptyLock(Lock);
	//return Nodes.empty();
	Lock.lock();
	bool Condition = Nodes.empty();
	Lock.unlock();
	return Condition;
};

*/

/*
Better wait thing:

Goal: wait for avaible node

A busy is wait is bad

So basically, we should have a wait unil a new node gets pushed onto the stack, or if a worker thread is now waiting

A conditional variable can be used for both of these things. Each time it gets signaled, we first check for a new new
node. If a new node is there, we first mark the thread as no longer waiting, take the node, and then process it
But if there is no node, we check if all other threads are waiting. If they are, we return from teh function. Otherwise,
we start waiting again

while true
	lock stack mutex 
	if node is on stack
		pop node
		unlock mutex
		break
	else
		unlock mutex
		set threading to waiting
		if is first iteration of loop 
			signal all other threads // to check if all threads are waitng
		if all other threads are waiting 
			return
		wait

process node
lock stack mutex
push children
unlock stack mutex
signal all other threads

unlock once:

bool popped = false

lock mtx

if stack no empty
	popped = true
	pop stack

unlock mtx

if(!popped)
	[...]

But slower since two ifs
*/

/*
		bool WaitingForNextNode = true;
		while (WaitingForNextNode) {

			ConstructionMutex.lock();
			if (!ConstructionNodes.empty()) {
				CurrentNode = ConstructionNodes.top();
				ConstructionNodes.pop();
				WaitingForNextNode = false;
			}
			ConstructionMutex.unlock();

			*ThreadWait = WaitingForNextNode;
			if (AllThreadWaitingConditions(WaitConditions)) {
				return;
			}
		}
*/

/*
Split Split::operator=(const Split& Split2) {
	Split Split1;

	Split1.Axis = Split2.Axis;
	Split1.SAH = Split2.SAH;

	Split1.Box[0] = Split2.Box[0];
	Split1.Box[1] = Split2.Box[1];

	Split1.Centroids[0] = Split2.Centroids[0];
	Split1.Centroids[1] = Split2.Centroids[1];

	return Split1;
}
*/

/*
	// We only care about on dimension, so copy everything to a sortable thing first for improved cache efficeny and iteration and array lookups

	struct TriangleSortCentroid {
		TriangleSortCentroid(const TriangleCentroid& TriCtr, uint32_t IterIdx, uint32_t IdxAxis) :
			AxisPos(TriCtr.Position[IdxAxis]),
			Index  (IterIdx) {}

		float AxisPos;
		uint32_t Index;
	};

	std::vector<TriangleSortCentroid> CentroidList;
	CentroidList.reserve(Centroids.size());

	uint32_t IterIdx = 0;
	for (const TriangleCentroid& TriCtr : Centroids) {
		CentroidList.emplace_back(TriCtr, IterIdx++, Axis);
	}

	std::sort(
		CentroidList.begin(), CentroidList.end(),
		[Axis](const TriangleSortCentroid& Arg0, const TriangleSortCentroid& Arg1) -> bool {
			return Arg0.AxisPos < Arg1.AxisPos;
		}
	);

	// Rebuild centroid list

	std::vector<TriangleCentroid> RebuiltList;
	RebuiltList.reserve(CentroidList.size());

	for (const TriangleSortCentroid& TriCtr : CentroidList) {
		RebuiltList.push_back(Centroids[TriCtr.Index]);
	}

	return RebuiltList;
*/

/*

Revised algorithm:

We first set up 2 centroid pools 
The first one has the sorted centroids, the other one is empty
It's not a good idea to have an empty node so we loop from 
1 centroid in the second pool until there's one centroid in the other pool
As we do this, we record which centroid was transferred to get the best
SAH. Once we are done looping, we reconstruct the best split

We will use do while to get at least something that is useable

integer best index = 0, current index = 0

do
	transfer split

	rate split

	if this split is better than last split
		best index = current index
	current index ++
while !pool[0].empty

*/

/*
inline Split FindBestSplit(const std::vector<TriangleCentroid>& Centroids, const std::vector<AABB>& TriangleBoundingBoxes, uint32_t Axis, uint32_t PreviousAxis) {
	assert(Centroids.size() - 1);

	std::vector<TriangleCentroid> SortedCentroidArray = Axis == PreviousAxis ? Centroids : SortAxis(Centroids, Axis);

	Split CurrentSplit;
	CurrentSplit.Centroids[0] = SortedCentroidArray;
	CurrentSplit.Centroids[1].reserve(SortedCentroidArray.size());

	std::vector<AABB> BoundingBoxes = PrecomputeBoundingBoxes(Centroids, TriangleBoundingBoxes, CurrentSplit.Centroids[0].size() - 1);

	float BestSplitSAH = FLT_MAX;
	size_t BestSplitIndex = 0;

	AABB BestBox[2];

	for (size_t CentroidIndex = 0; CurrentSplit.Centroids[0].size() > 1; CentroidIndex++) {
		// Transfer the centroid
		TriangleCentroid LastCentroid = CurrentSplit.Centroids[0].back();
		CurrentSplit.Centroids[0].pop_back();
		CurrentSplit.Centroids[1].push_back(LastCentroid);

		// Create bounding boxes and everything.
		CurrentSplit.Box[0] = BoundingBoxes[CentroidIndex]; // This might benfit from a check of whether the centroid had an effect on the bounding box
		CurrentSplit.Box[1].Extend(TriangleBoundingBoxes[LastCentroid.Index]);

		// Compute SAH and update if it's better than the current best split
		if (CurrentSplit.ComputeSAH() < BestSplitSAH) {
			BestSplitSAH = CurrentSplit.SAH;
			BestSplitIndex = CentroidIndex;

			BestBox[0] = CurrentSplit.Box[0];
			BestBox[1] = CurrentSplit.Box[1];
		}
	}

	Split BestSplit;
	BestSplit.Axis = Axis;
	BestSplit.SAH = BestSplitSAH;

	BestSplit.Box[0] = BestBox[0];
	BestSplit.Box[1] = BestBox[1];

	BestSplit.Centroids[0].reserve(BestSplitIndex);
	BestSplit.Centroids[1].reserve(CurrentSplit.Centroids[0].size() - 1 - BestSplitIndex);

	std::copy_n(SortedCentroidArray.begin(), BestSplitIndex, std::back_inserter(BestSplit.Centroids[0]));
	std::copy(SortedCentroidArray.begin() + BestSplitIndex, SortedCentroidArray.end(), std::back_inserter(BestSplit.Centroids[1]));

	return BestSplit;
}

			AABB leftBoxBuilder;
			std::vector<AABB> leftBoxes;
			for (uint32_t j = 0; j < node.references.size() - 1; j++) {
				leftBoxBuilder.Extend(node.references[i].box);
				leftBoxes.push_back(leftBoxBuilder);
			}

			AABB rightBox;
			BuilderNode left, right;
			for (uint32_t j = node.references.size() - 1; j >= 0; j--) {
				AABB leftBox = leftBoxes[j - 1];
				rightBox.Extend(node.references[j].box);
				float sah = leftBox.SurfaceArea() * (j - 1) + rightBox.SurfaceArea() * (node.references.size() - j);
				if (sah < bestSah) {
					for (uint32_t k = 0; k < j; k++) {
						left.Insert(node.references[k]);
					}

					for (uint32_t k = j; k < node.references.size(); k++) {
						right.Insert(node.references[k]);
					}

					bestLeft = left;
					bestRight = right;
					bestSah = sah;
				}
			}

*/

constexpr int kNumBins = 8;
int ComputeBinID(float c, float k0, float k1) {
	float proj = k1 * (c + k0);
	int unbounded = (int)proj;
	return clamp(unbounded, 0, (int)kNumBins - 1);
}

struct TriangleReference {
	int index;
	AABB box;
	vec3 centroid;
};

struct ReferenceContainer {
	AABB box;
	std::vector<TriangleReference> references;
	int numReferences;

	ReferenceContainer() : numReferences(0) {}

	void Insert(const TriangleReference& reference) {
		references.push_back(reference);
		box.Extend(reference.box);
		numReferences++;
	}

	void Reset() {
		AABB clear;
		box = clear;

		numReferences = 0;
		references.clear();
	}
};

struct Bin {
	AABB box;
	int numReferences;
	Bin() : numReferences(0) {}
	void Insert(const TriangleReference& reference) {
		box.Extend(reference.box);
		numReferences++;
	}
};

struct BuilderNode : public ReferenceContainer {
	BuilderNode* children;
	int offset;

	BuilderNode() : children(nullptr), offset(0) {}

	// For lack of a better word (the nodes "consume" the bins
	void Consume(const Bin& bin) {
		box.Extend(bin.box);
	}

	float ComputeSAH() const {
		return box.SurfaceArea() * numReferences;
	}

	// Debug information
	int id;
	int depth;
};

void FindBestObjectSplit(BuilderNode& bestLeft, BuilderNode& bestRight, float& bestSah, BuilderNode& node) {
	// Check if it is more optimal to do sweeping or binning
	if (node.references.size() > kNumBins) {

		bool betterSplitFound = false;
		int bestBin = 1, bestAxis = 0;
		std::vector<Bin[kNumBins]> bins(3);

		// Consider each axis
		for (int i = 0; i < 3; i++) {

			// Extents of our parent node's box, which we will subdivide in the binning process
			float minBox = node.box.min[i];
			float maxBox = node.box.max[i];
			if (minBox == maxBox) { // Prevent binning on a flat axis
				continue;
			}

			// Precalculate binning information (see Wald 2007, Section 3.3)
			float k0 = -minBox;
			float k1 = kNumBins / (maxBox - minBox);

			// Initialize our bins
			for (const auto& ref : node.references) {
				// Find the correct bin and insert the reference
				int binID = ComputeBinID(ref.centroid[i], k0, k1);
				bins[i][binID].Insert(ref);

			}

			// For each bin, there are k - 1 splits: [0, i) for the left and [i, kNumBins) for the right. Precompuation and iterative extension allows us to have O(n) complexity where n is number of bins

			// Precompute our right AABBs 
			AABB rightBoxBuilder;
			std::vector<AABB> rightPrecomputedBoxes;
			rightPrecomputedBoxes.reserve(kNumBins);
			for (int j = kNumBins - 1; j >= 1; j--) {
				rightBoxBuilder.Extend(bins[i][j].box);
				rightPrecomputedBoxes.push_back(rightBoxBuilder);
			}

			// Loop through all the splits
			BuilderNode left;
			for (int j = 1; j < kNumBins; j++) {
				// Iteratively extend the left node
				left.Consume(bins[i][j - 1]);
				left.numReferences += bins[i][j - 1].numReferences;

				// Grab our right node from our precomputed information
				BuilderNode right;
				right.box = rightPrecomputedBoxes[kNumBins - j - 1];
				right.numReferences = node.numReferences - left.numReferences;

				// Update our best split if necessary
				float sah = left.ComputeSAH() + right.ComputeSAH();
				if (sah < bestSah) {
					bestLeft = left;
					bestRight = right;
					bestSah = sah;
					bestBin = j;
					bestAxis = i;
					betterSplitFound = true;
				}
			}
		}

		// If we found a split that improves SAH, then we will actually subdivide using it
		if (betterSplitFound) {
			// First, clear any junk information from a worse split
			bestLeft.Reset();
			bestRight.Reset();

			// Precompute world-to-bin space mapping variables
			float minBox = node.box.min[bestAxis];
			float maxBox = node.box.max[bestAxis];
			float k0 = -minBox;
			float k1 = kNumBins / (maxBox - minBox);

			// Loop through all references
			for (const auto& ref : node.references) {
				int binID = ComputeBinID(ref.centroid[bestAxis], k0, k1); // Find which bin our reference is in
				// Insert into appropriate child
				if (binID < bestBin) {
					bestLeft.Insert(ref);
				}
				else {
					bestRight.Insert(ref);
				}
			}
		}
		
	}
	else if (node.references.size() > 1) {
		// Give that we have a few references, we should sweep instead

		// Clear any junk from beforehand
		bestLeft.Reset();
		bestRight.Reset();

		// Reserve the maximum amount of memory we will be using
		bestLeft.references.reserve(kNumBins - 1);
		bestRight.references.reserve(kNumBins - 1);

		// Consider each axis
		for (int i = 0; i < 3; i++) {
			// Sort each reference according to its centroid on the axis
			std::sort(node.references.begin(), node.references.end(), [i](const TriangleReference& lhs, const TriangleReference& rhs) {return lhs.centroid[i] < rhs.centroid[i]; });

			// Precompute our right AABBs
			AABB rightBoxBuilder;
			std::vector<AABB> rightPrecomputedBoxes;
			rightPrecomputedBoxes.reserve(kNumBins);
			for (auto iter = node.references.rbegin(); iter != node.references.rend(); iter++) {
				rightBoxBuilder.Extend(iter->box);
				rightPrecomputedBoxes.push_back(rightBoxBuilder);
			}
			std::reverse(rightPrecomputedBoxes.begin(), rightPrecomputedBoxes.end());

			// Loop through all splits
			BuilderNode left;
			for (int j = 0; j < node.numReferences - 1; j++) {
				// Determine what our nodes look like in this split
				left.Insert(node.references[j]);
				AABB right = rightPrecomputedBoxes[j + 1];

				// If we have a better SAH, then subdivide the node
				float sah = left.box.SurfaceArea() * left.numReferences + right.SurfaceArea() * (node.numReferences - left.numReferences);
				if (sah < bestSah) {
					bestSah = sah;
					bestLeft = left;
					bestRight.box = right;

					// TODO: avoid resubdivision of splits (since we many now subdivide and may later find a better split, so we did useless work now)
					bestRight.numReferences = 0;
					bestRight.references.clear();
					for (int k = j + 1; k < node.references.size(); k++) {
						bestRight.Insert(node.references[k]);
					}
				}
			}
		}
	}
}

void DebugNode(const BuilderNode& input, const char* name) {
	auto node = input;
	node.box.max -= node.box.min;
	std::cout << "Node information for " << name << ":\n";
	std::cout << "Depth:\t" << node.depth << '\n';
	std::cout << "ID:\t" << node.id << '\n';
	std::cout << "Number of references:\t" << node.numReferences << '\n';
	std::cout << "Miniumum extents:\t" << node.box.min.x << '\t' << node.box.min.y << '\t' << node.box.min.z << '\n';
	std::cout << "Maxiumum extents:\t" << node.box.max.x << '\t' << node.box.max.y << '\t' << node.box.max.z << '\n';
	std::cout << "Surface area: " << node.box.SurfaceArea() << '\n';
	std::cout << '\n';
}

AABB Overlap(const AABB& lhs, const AABB& rhs) {
	AABB o;
	o.min = max(lhs.min, rhs.min);
	o.max = max(lhs.max, rhs.max);
	return o;
}

// Intersection of 2 1D lines
TriangleReference ClipReference(const TriangleReference& ref, int axis, float lower, float upper) {
	TriangleReference clipped = ref;
	clipped.box.min[axis] = max(clipped.box.min[axis], lower);
	clipped.box.max[axis] = min(clipped.box.max[axis], upper);
	clipped.centroid = clipped.box.Center();
	return clipped;
}

void FindBestSpatialSplit(BuilderNode& bestLeft, BuilderNode& bestRight, float& bestSah, const BuilderNode& node) {
	/*
	TODO: find out why I am not getting a performance boost large as advertised in Stich at al 
	TODO: optimize construction

	To do spatial splits and have an easy time avoiding reference duplication within a node, we need to maintain 2 counters in each of our bins:
	1. The min counter, and
	2. The max counter
	These counters are referred to as the entry and exit counters in the SBVH paper respectively.
	The minimum counter maintains how many references begin along the projected axis in the current bin
	The maximum counter maintains how many references end along the projected axis in the current bin
	For all bins in [min, max], we "chop" up the references bounding box (or just use the the clipped triangle's AABB instead for quality over build speed) and extend our bins' AABB by it
	To create binned splits using the min-max binning algorithm, the left child scoops up the min bins, while the right child scoops up the right bins
	We can then cheaply evalute SAH using this, to find the best split between our bins
	Note that we do not actually keep track of reference lists, since binning is only used to keep track of how many references are where and the AABBs of our bins
	At the end, we use the best split to actually subdivide the references

	The chopping process:
	if we are binning on a certain axis, for instance, the z axis, we know that the chopped AABB will be different in the z axis since the xy axes are unaffected
	We simply restrict z to the bounds of the plane

	For more details, I highly recomend reading section 3 of "Highly Parallel Fast KD-tree Construction for Interactive Ray Tracing of Dynamic Scenes"
	Following along with the paper, I use the "min-max" naming convention in my code

	Now some information regarding the SBVH algorithm:
	SBVH is mostly just like regular binning except there are 3 key differences. Let's start with how regular binning works before moving onto SBVH

	First of all, it is assumed that our BVH builder works over references to primitives, ie, a portion of the primitive is enclosed in a bounding box. This makes construction simpler
	Regular binning [Wald 2007] works by distributing references into "bins" according to their centroid. Each bin counts the number of references with in and how large the combined AABBs are
	The bins are distributed at intervals along a given axis. They can be thought of as histograms in a way
	We then sweep over these bins instead over all references and find the cheapest split according to its SAH value. Once we have found it, we generete the split by inputting references into their respective bins
	The authors of the SBVH paper refer to sweeping and binning in this regard as object partioning

	The authors of the SBVH paper also observed that object partioning suffers from the same problem of non-uniformally tesselated geometry
	Non-uniformly tesselated geometry is when you have a few large triangles and many small triangles to process in a given split.
	The issue here is that the few big triangles pose an issue when it comes time to build hte AABBs, resulting in horrible SAH

	A workaround that the authors proposed is to split the reference into two parts: one behind the split and one ahead of it
	This way the big triangles will not impact our BVH construction.

	To implement this, we first have to initial our bins with spatially split references. We then iterate over the possible combinations of splits and choose the best one. We then subsdivide the split


	*/

	// Shevstov et al. 2007 and Stich et al. 200?
	struct MinMaxBin {
		AABB box; // Like regular BVH binning, we keep track of our bounds [Wald 2007]
		int minReferences; // Referred to as "entry" in Stich et al 
		int maxReferences; // Referred to as "exit" in Stich et al
		MinMaxBin() : minReferences(0), maxReferences(0) {}
	};

	int nR, nL;
	AABB bL, bR;
	// Although we consider each axis for the split, we can speed things up by considering the longest axis only
	for (int i = 0; i < 3; i++) {
		// Extents of our parent node's box, which we will subdivide in the binning process
		float minBox = node.box.min[i];
		float maxBox = node.box.max[i];
		if (minBox == maxBox) { // Prevent binning on a flat axis
			continue;
		}

		// Precalculate binning information (see Wald 2007, Section 3.3)
		float k0 = -minBox;
		float k1 = kNumBins / (maxBox - minBox);
		float binWidth = (maxBox - minBox) / kNumBins;
		// Iterate through all references
		MinMaxBin bins[kNumBins]; // TODO: use a different number of bins for spatial splits

		int sMin = 0, sCen = 0, sMax = 0;
		for (const auto& ref : node.references) {
			// Find our min and max bins
			int minID = ComputeBinID(ref.box.min[i], k0, k1);
			int maxID = ComputeBinID(ref.box.max[i], k0, k1);
			sMin += minID;
			sCen += ComputeBinID(ref.centroid[i], k0, k1);
			sMax += maxID;
			// Increment to mark our reference's entry and exit
			bins[minID].minReferences++;
			bins[maxID].maxReferences++;
			// For each bin in [b_min, b_max], we need to extend it by the clipped box
			for (int j = minID; j <= maxID; j++) {
				// Calculate how far our bins will be restricted
				float lowerPlane = minBox + binWidth * j;
				float upperPlane = minBox + binWidth * (j + 1);
				// Create our clipped AABB
				AABB clipped = ref.box;
				clipped.min[i] = max(clipped.min[i], lowerPlane);
				clipped.max[i] = min(clipped.max[i], upperPlane);
				// Extend our bin
				bins[j].box.Extend(clipped);
			}
		}

		// Now that we have our bins, let's consider each spatial split
		for (int j = 1; j < kNumBins; j++) {
			BuilderNode left, right;

			// Build our left node
			for (int k = 0; k < j; k++) {
				left.box.Extend(bins[k].box);
				left.numReferences += bins[k].minReferences;
			}

			// Build our right node
			for (int k = j; k < kNumBins; k++) {
				right.box.Extend(bins[k].box);
				right.numReferences += bins[k].maxReferences;
			}

			// Update our split if it is better
			float sah = left.ComputeSAH() + right.ComputeSAH();
			if (sah < bestSah) {
				bestSah = sah;

				nL = left.numReferences;
				nR = right.numReferences;

				bL = left.box;
				bR = right.box;

				bestLeft.Reset();
				bestRight.Reset();

				float split = minBox + binWidth * j;
				//std::cout << j << '\n';
				for (const auto& ref : node.references) {
					// Since we binned using bin IDs, we must also subdivide using bin IDs to prevent some sort of weirdness
					int minID = ComputeBinID(ref.box.min[i], k0, k1);
					int maxID = ComputeBinID(ref.box.max[i], k0, k1);
					// Determine whether we need to split the reference or not
					if (minID < j && maxID >= j) {
						// Our reference goes through the split and therefore needs to be split
						// However, we can try unsplitting it instead if it is a better method (see Stich et al, Section 4.4)

						AABB extendL = bL;
						AABB extendR = bR;

						extendL.Extend(ref.box);
						extendR.Extend(ref.box);

						float unsplitL = extendL.SurfaceArea() * nL + bR.SurfaceArea() * (nR - 1);
						float unsplitR = bR.SurfaceArea() * (nL - 1) + extendR.SurfaceArea() * nR;

						if (unsplitL < bestSah && unsplitL < unsplitR) {
							bestLeft.Insert(ref);
						}
						else if (unsplitR < bestSah && unsplitR < unsplitL) {
							bestRight.Insert(ref);
						}
						else {
							auto leftRef = ClipReference(ref, i, -FLT_MAX, split);
							bestLeft.Insert(leftRef);

							auto rightRef = ClipReference(ref, i, split, FLT_MAX);
							bestRight.Insert(rightRef);
						}
					}
					else {
						// Our reference does not go through the split and therefore can be inserted as is into on child only
						if (minID < j) {
							bestLeft.Insert(ref); // Choose the left child
						}
						else {
							bestRight.Insert(ref); // Choose the right child
						}
					}
				}
			}
		}
	}
}

int numLeafReferences = 0;
int numLeafs = 0;
int depthSum = 0;
std::vector<int> BuildSBVH(std::vector<CompactTriangle>& triangles, BuilderNode* root) {
	int id = 0;
	root->id = id++;
	// The first node we need to process is the root
	std::vector<int> references;
	std::stack<BuilderNode*> unprocessedSubtrees;
	unprocessedSubtrees.push(root);

	constexpr float alpha = 1e-5f;
	float spatialInefficiencyThreshold = alpha * root->box.SurfaceArea();

	// Loop through each unprocessed subtree and subdivide it or make it a leaf
	while (!unprocessedSubtrees.empty()) {

		BuilderNode& node = *unprocessedSubtrees.top();
		unprocessedSubtrees.pop();
		//std::cout << node.depth << '\t';

		if (node.depth > 48) {
			std::cout << "Tree too deep!\n";
			exit(-1);
		}

		if (node.depth > 32) {
			DebugNode(node, "deep tree alert");
		}

		BuilderNode bestLeft, bestRight;
		float bestSah = FLT_MAX;

		// Find our best object partioning canidate
		FindBestObjectSplit(bestLeft, bestRight, bestSah, node);

		AABB overlap;
		overlap.min = max(bestLeft.box.min, bestRight.box.min);
		overlap.max = min(bestLeft.box.max, bestRight.box.max);

		if (overlap.SurfaceArea() > spatialInefficiencyThreshold) {
			FindBestSpatialSplit(bestLeft, bestRight, bestSah, node);
		}

		// Subdivide our node if it is efficient to do so
		if (bestSah < node.ComputeSAH()) {
			node.children = new BuilderNode[2];
			node.children[0] = bestLeft;
			node.children[1] = bestRight;

			node.children[0].depth = node.depth + 1;
			node.children[1].depth = node.depth + 1;

			node.children[0].id = id++;
			node.children[1].id = id++;

			unprocessedSubtrees.push(&node.children[0]);
			unprocessedSubtrees.push(&node.children[1]);

			node.references.clear();
			if (node.id == -1) {
				exit(0);
			}
		}
		else {
			// Our traversal expects to handle duplicated references, so we must have a second reference buffer that points to locations in our triangle buffer
			// This doesn't terrible affect caching performance, as measured in my expriments where I created a new triangle buffer to perfectly match the references to create an upper bound on caching (or at least a very close one)
			// The difference was negligible, and I hypothesize that this is a result of most leaves having one triangle
			// However, a possible way to further improver performance is to reorder according to spatial locality, which is a big win for coherent rays
			node.offset = references.size();
			for (const auto& ref : node.references) {
				references.push_back(ref.index);
			}

			numLeafReferences += node.numReferences;
			numLeafs++;
			depthSum += node.depth;
		}
	}

	return references;
}

void BoundingVolumeHierarchy::BuildBinnedSpatial(std::vector<CompactTriangle>& triangles) {
	// Build the BVH over references
	BuilderNode root;
	for (int i = 0; i < triangles.size(); i++) {
		TriangleReference reference;
		reference.index = i;

		reference.box.Extend(triangles[i].position0);
		reference.box.Extend(triangles[i].position1);
		reference.box.Extend(triangles[i].position2);

		reference.centroid = reference.box.Center();

		root.Insert(reference);
	}

	root.depth = 0;
	auto references = BuildSBVH(triangles, &root);

	std::cout << "Reference duplication is " << (float)numLeafReferences / root.numReferences << "%\n";
	std::cout << "Average refs per leaf is " << (float)numLeafReferences / numLeafs << " refs\n";
	std::cout << "Average depth of leaf is " << (float)depthSum / numLeafs << '\n';

	/*
	Recorded stats using SBVH on breakfast room scene:
		Reference duplication is 1.07023%
		Average refs per leaf is 1.03309 refs
		Average depth of leaf is 21.591
		Average FPS is 14-ish
	SBVH without fix to deep trees:
		Reference duplication is 1.09782%
		Average refs per leaf is 1.03572 refs
		Average depth of leaf is 21.6914
		Average FPS is 14-ish (no change from deep tree fix)
	Only binned BVH as in Wald 2007
		Reference duplication is 1%
		Average refs per leaf is 1.02927 refs
		Average depth of leaf is 20.5308
		Average FPS is 17, for some odd reason higher than SBVH

	deep-tree, conf room: 45 FPS
	bin const, conf room:
	*/

	// Serealize our nodes
	std::vector<BuilderNode*> allocationRegistry;
	std::vector<NodeSerialized> serealizedNodes;
	std::queue<BuilderNode*> bfs;
	bfs.push(&root);
	while (!bfs.empty()) {

		BuilderNode& buildOutput = *bfs.front();
		bfs.pop();

		NodeSerialized gpuReadableNode;

		gpuReadableNode.BoundingBox = buildOutput.box;
		gpuReadableNode.parent = 0;

		if (buildOutput.children != nullptr) {
			gpuReadableNode.firstChild = serealizedNodes.size() + bfs.size() + 1;

			// Push box with largest surface area first
			if (buildOutput.children[0].box.SurfaceArea() < buildOutput.children[1].box.SurfaceArea()) {
				bfs.push(&buildOutput.children[1]);
				bfs.push(&buildOutput.children[0]);
			}
			else {
				bfs.push(&buildOutput.children[0]);
				bfs.push(&buildOutput.children[1]);
			}

			allocationRegistry.push_back(buildOutput.children);
		}
		else {
			gpuReadableNode.triangleRange = -((buildOutput.numReferences & 15) | (buildOutput.offset << 4));
		}

		serealizedNodes.push_back(gpuReadableNode);
	}

	for (auto& ptr : allocationRegistry) {
		delete[] ptr;
	}

	// Reorder nodes for better memory access on the GPU
	for (NodeSerialized& node : serealizedNodes) {
		struct NewLayout {
			vec3 min;
			int data0;
			vec3 max;
			int data1;
		} temp;

		temp.min = node.BoundingBox.min;
		temp.data0 = node.firstChild;
		temp.max = node.BoundingBox.max;
		temp.data1 = node.parent;

		NewLayout* ptr = (NewLayout*)&node;
		*ptr = temp;
	}

	nodesBuf.CreateBinding(BUFFER_TARGET_ARRAY);
	nodesBuf.UploadData(serealizedNodes, GL_STATIC_DRAW);

	nodesTex.CreateBinding();
	nodesTex.SelectBuffer(&nodesBuf, GL_RGBA32F);

	referenceBuf.CreateBinding(BUFFER_TARGET_ARRAY);
	referenceBuf.UploadData(references, GL_STATIC_DRAW);

	referenceTex.CreateBinding();
	referenceTex.SelectBuffer(&referenceBuf, GL_R32F);
}


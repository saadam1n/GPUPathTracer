/*
TLDR for the BVH construction algorithm:
We create a split between all midpoints for centroids of the triangle for all 3 axes
We then rate each split using the SAH and choose the one with the lowest SAH 
Then we serealize it
*/

#include "BVH.h"
#include <stack>
#include <algorithm>
#include <list>
#include <queue>
#include <thread>

using BVH = BoundingVolumeHierarchy;

Triangle AssembleTriangle(const Vertex* Vertices, const TriangleIndexData& TriIdxInfo) {
	Triangle CurrentTriangle;

	CurrentTriangle.Vertices[0] = Vertices[TriIdxInfo.Indices[0]];
	CurrentTriangle.Vertices[1] = Vertices[TriIdxInfo.Indices[1]];
	CurrentTriangle.Vertices[2] = Vertices[TriIdxInfo.Indices[2]];

	return CurrentTriangle;
}

NodeUnserialized::NodeUnserialized(void) : Children{ nullptr, nullptr }, Type(NodeType::NODE) {}

Split::Split(void) : SAH(FLT_MAX) {}

float Split::ComputeSAH(void) {
	SAH = Box[0].SurfaceArea() * Centroids[0].size() + Box[1].SurfaceArea() * Centroids[1].size();
	return SAH;
}

/*
First, build root node
Second, allocate 2 child nodes and set up refrences so you don't have to deal with them
Third, partition triangles to each node
Fourth, process each node as in step 2
*/

void BoundingVolumeHierarchy::ConstructAccelerationStructure(const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices, uint32_t DepthHint, uint32_t LeafHint) {
	// First things first. We need to build a list of triangles

	std::vector<TriangleCentroid> CentroidList;
	CentroidList.reserve(Indices.size());

	for (uint32_t TriIdx = 0; TriIdx < Indices.size(); TriIdx++) {
		TriangleIndexData TriIdxInfo = Indices[TriIdx];

		Triangle CurrentTriangle = AssembleTriangle(Vertices.data(), TriIdxInfo);

		// Calculate centriod of the triangle. We need this in order to split triangles in the BVH building process
		TriangleCentroid Centroid;

		Centroid.Index = TriIdx;

		Centroid.Position =
			CurrentTriangle.Vertices[0].Position +
			CurrentTriangle.Vertices[1].Position +
			CurrentTriangle.Vertices[2].Position ;

		Centroid.Position /= 3.0f;

		CentroidList.push_back(Centroid);
	}



	// TODO: pre-sorting
	// std::vector<TriangleCentroid> SortedCentroidList[3];

	// SortedCentroidList[0] = SortAxis(CentroidList, 0);
	// SortedCentroidList[1] = SortAxis(CentroidList, 1);
	// SortedCentroidList[2] = SortAxis(CentroidList, 2);

	//struct SortedCentroidReference {

	//};

	// After building the list of triangles, we need to build the BVH

	Depth = DepthHint;

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
	struct ConstructionNode {
		NodeUnserialized* DataPtr;
		int32_t Depth;
	};

	std::stack<ConstructionNode> ConstructionNodeStack;

	/*
	Index buffer
	*/
	std::vector<int32_t> LeafContentBuffer;

	/*
	First we need to construct the root node and push it onto the stack
	The root node is nothing to complicated
	It's literally just the same as computing a bounding box
	*/

	NodeUnserialized* RootNode = new NodeUnserialized;

	RootNode->BoundingBox = CreateBoundingBox(CentroidList, Vertices, Indices);
	RootNode->Centroids = CentroidList;

	ConstructionNode CRN;

	CRN.DataPtr = RootNode;
	CRN.Depth = 0;

	ConstructionNodeStack.push(CRN);

	while (!ConstructionNodeStack.empty()) {
		// Fetch the next node from the stack
		ConstructionNode CurrentNode = ConstructionNodeStack.top();
		ConstructionNodeStack.pop();

		// Check if node can be a leaf
		if (CurrentNode.DataPtr->Centroids.size() < LeafHint || (uint32_t)CurrentNode.Depth > DepthHint) {
			CurrentNode.DataPtr->Type = NodeType::LEAF;

			CurrentNode.DataPtr->Leaf.Offset = (int32_t)LeafContentBuffer.size()     ;
			CurrentNode.DataPtr->Leaf.Size   = (int32_t)CurrentNode.DataPtr->Centroids.size();

			for (const TriangleCentroid& Tri : CurrentNode.DataPtr->Centroids) {
				LeafContentBuffer.push_back(Tri.Index);
			}
		} else {
			// Next try to find the best split on all 3 axes
			Split TentativeSplits[3];

			TentativeSplits[0] = FindBestSplit(CurrentNode.DataPtr->Centroids, Vertices, Indices, 0);
			TentativeSplits[1] = FindBestSplit(CurrentNode.DataPtr->Centroids, Vertices, Indices, 1);
			TentativeSplits[2] = FindBestSplit(CurrentNode.DataPtr->Centroids, Vertices, Indices, 2);
			// Find/selelct the best split from all axes 
			Split BestSplit = ChooseBestSplit(TentativeSplits[0], TentativeSplits[1], TentativeSplits[2]);
			// Construct children nodes
			ConstructionNode Children[2];

			Children[0].DataPtr = new NodeUnserialized;
			Children[1].DataPtr = new NodeUnserialized;

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

	std::queue<NodeUnserialized*> IndexBuildingQueue;
	IndexBuildingQueue.push(RootNode);

	int32_t IndexCounter = 0;

	while (!IndexBuildingQueue.empty()) {
		NodeUnserialized* CurrentNode = IndexBuildingQueue.front();
		IndexBuildingQueue.pop();

		CurrentNode->Index = IndexCounter++;

		if (CurrentNode->Type == NodeType::NODE) {
			IndexBuildingQueue.push(CurrentNode->Children[0]);
			IndexBuildingQueue.push(CurrentNode->Children[1]);
		}
	}

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

		if (CurrentNode->Type == NodeType::NODE) {
			SerializedNode.ChildrenNodes[0] = CurrentNode->Children[0]->Index;
			SerializedNode.ChildrenNodes[1] = CurrentNode->Children[1]->Index;

			IndexConnectionQueue.push(CurrentNode->Children[0]);
			IndexConnectionQueue.push(CurrentNode->Children[1]);
		} else {
			SerializedNode.Leaf = CurrentNode->Leaf;
			SerializedNode.MakeLeaf();
		}
		ProcessedNodes.push_back(SerializedNode);

		delete CurrentNode;
	}
	
	//DebugPrintBVH(ProcessedNodes, LeafContentBuffer);

	StructureBuffers.Nodes.CreateBinding(BUFFER_TARGET_ARRAY);
	StructureBuffers.Nodes.UploadData(ProcessedNodes.size() * sizeof(NodeSerialized), ProcessedNodes.data());

	Samplers.Nodes.CreateBinding();
	Samplers.Nodes.SelectBuffer(&StructureBuffers.Nodes, GL_RGBA32F);

	StructureBuffers.Leaves.CreateBinding(BUFFER_TARGET_ARRAY);
	StructureBuffers.Leaves.UploadData(LeafContentBuffer.size() * sizeof(int32_t), LeafContentBuffer.data());

	Samplers.Leaves.CreateBinding();
	Samplers.Leaves.SelectBuffer(&StructureBuffers.Leaves, GL_RGB32I);
}

void NodeSerialized::MakeLeaf(void) {
	// Use negative indices to differentiate between leaves and nodes. We use size here since size shouldn't be 0 but offset could be 0.
	Leaf.Size = -Leaf.Size;
	// Decrement to fix weird triangle front face culling bug
	// Leaf.Offset--;
}

NodeType NodeSerialized::GetType(void) {
	return Leaf.Size < 0 ? NodeType::LEAF : NodeType::NODE;
}

AABB BoundingVolumeHierarchy::CreateBoundingBox(const std::vector<TriangleCentroid>& Centroids, const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices) {
	// Loop through all centroids and get their triangle to create box
	AABB Box;

	for (const TriangleCentroid& Centroid : Centroids) {
		Triangle CurrentTriangle = AssembleTriangle(Vertices.data(), Indices[Centroid.Index]);

		Box.Extend(CurrentTriangle.Vertices[0].Position);
		Box.Extend(CurrentTriangle.Vertices[1].Position);
		Box.Extend(CurrentTriangle.Vertices[2].Position);
	}

	return Box;
}

std::vector<TriangleCentroid> BoundingVolumeHierarchy::SortAxis(const std::vector<TriangleCentroid>& Centroids, uint32_t Axis) {
	std::vector<TriangleCentroid> Sorted(Centroids);

	std::sort (
		Sorted.begin(), Sorted.end(),
		[Axis](const TriangleCentroid& Arg0, const TriangleCentroid& Arg1) -> bool {
			return Arg0.Position[Axis] < Arg1.Position[Axis];
		}
	);

	return Sorted;
}

Split BoundingVolumeHierarchy::FindBestSplit(const std::vector<TriangleCentroid>& Centroids, const std::vector<Vertex>& Vertices, const std::vector<TriangleIndexData>& Indices, uint32_t Axis) {
	// Ensure sorted, later I will presort once at the beginning. I also could do all split calculations in the same loop
	std::vector<TriangleCentroid> SortedCentroids = SortAxis(Centroids, Axis);

	Split BestSplit;

	for (auto Iter = SortedCentroids.begin(); Iter != SortedCentroids.end() - 1; Iter++) {
		auto NextIter = Iter + 1;

		float Midpoint = (Iter->Position[Axis] + NextIter->Position[Axis]) * 0.5f;

		Split CurrentSplit;

		for (const TriangleCentroid& Centroid : Centroids) {
			if (Centroid.Position[Axis] < Midpoint) {
				CurrentSplit.Centroids[0].push_back(Centroid);
			} else {
				CurrentSplit.Centroids[1].push_back(Centroid);
			}
		}

		CurrentSplit.Box[0] = CreateBoundingBox(CurrentSplit.Centroids[0], Vertices, Indices);
		CurrentSplit.Box[1] = CreateBoundingBox(CurrentSplit.Centroids[1], Vertices, Indices);

		if (CurrentSplit.ComputeSAH() < BestSplit.SAH) {
			BestSplit = CurrentSplit;
		}
	}

	return BestSplit;
}

// There's probably a more fast way to do this
Split BoundingVolumeHierarchy::ChooseBestSplit(const Split& X, const Split& Y, const Split& Z) {
	if (X.SAH > Y.SAH && X.SAH > Z.SAH) {
		return X;
	} else if (Y.SAH > X.SAH&& Y.SAH > Z.SAH) {
		return Y;
	} else {
		return Z;
	}
}

void IndentDebugBVH(int32_t TabCount) {
	for (int32_t Counter = 0; Counter < TabCount; Counter++) {
		printf("\t");
	}
}

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
}

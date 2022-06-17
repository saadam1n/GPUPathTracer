#ifndef BVH_TRAVERSE_GLSL
#define BVH_TRAVERSE_GLSL

#include "Structure.glsl"
#include "../geometry/Object.glsl"

struct BVHStackItem {
	int Children[2];
};

// BVH_Stack_Item BVH_Stack[64];

// For some reason, directly setting the type to generic node seems to have alignment issues
// I have heard that GPUs align structs and whatnot to 16-byte alignments
// That might have something to do here but I do not want to find the optimal solution to a trivial task like sampling from memory
layout(std430) buffer nodes {
	vec4 tree[];
};

BVHNodeRecursive GetNodeGenericAsRecursive(BVHNodeGeneric Generic) {
	BVHNodeRecursive Recursive;

	Recursive.Node = Generic.Node;

	Recursive.ChildrenNodes[0] = Generic.Data[0];
	Recursive.ChildrenNodes[1] = Generic.Data[1];

	return Recursive;
}

BVHNodeLeaf GetNodeGenericAsLeaf(BVHNodeGeneric Generic) {
	BVHNodeLeaf Leaf;

	Leaf.Node = Generic.Node;

	// Negate the data so we remove the differentiation marker
	Leaf.Leaf.Index      =  Generic.Data[0];
	Leaf.Leaf.IndexCount = -Generic.Data[1];

	return Leaf;
}

BVHNodeGeneric GetNode(uint Index) {
	BVHNodeGeneric Node;

	vec4 TextureData[2];

	Index *= 2;
	TextureData[0] = tree[Index];
	TextureData[1] = tree[Index + 1];

	Node.Node.BoundingBox.Min    = TextureData[0].xyz;

	Node.Node.BoundingBox.Max.x  = TextureData[0].w  ;
	Node.Node.BoundingBox.Max.yz = TextureData[1].xy ;

	Node.Data[0] = floatBitsToInt(TextureData[1].z);
	Node.Data[1] = floatBitsToInt(TextureData[1].w);

	return Node;
}

BVHNodeRecursive GetRootNode() {
	BVHNodeRecursive Root = GetNodeGenericAsRecursive(GetNode(0));

	return Root;
}

BVHNodeGeneric GetChildNode(in BVHNodeRecursive ParentNode, in uint Index) {
	BVHNodeGeneric Child;

	Child = GetNode(ParentNode.ChildrenNodes[Index]);

	return Child;
}

BVHNodeGeneric GetRecursiveNodeAsGeneric(in BVHNodeRecursive R){
	BVHNodeGeneric G;

	G.Node = R.Node;

	G.Data[0] = R.ChildrenNodes[0];
	G.Data[1] = R.ChildrenNodes[1];

	return G;
}

// AABB intersection test by madmann
vec2 IntersectNode(in BVHNodeGeneric Node, in Ray InverseRay, in HitInfo Intersection) {
	vec3 t_node_min = Node.Node.BoundingBox.Min * InverseRay.Direction + InverseRay.Origin;
	vec3 t_node_max = Node.Node.BoundingBox.Max * InverseRay.Direction + InverseRay.Origin;

	vec3 t_min = min(t_node_min, t_node_max);
	vec3 t_max = max(t_node_min, t_node_max);

	float t_entry = max(t_min.x, max(t_min.y,     t_min.z                     ));
	float t_exit  = min(t_max.x, min(t_max.y, min(t_max.z, Intersection.Depth )));
	return vec2(t_entry, t_exit);
}

// assert intersection
bool ValidateIntersection(vec2 Distances) {
	return Distances.x <= Distances.y && Distances.y > 0.0f;
}

// Go to the next avaible node on the stack if there is any
//#define TRAVERSE_STACK()  if (StackIndex == -1) {break;} else {CurrentNode = Stack[StackIndex--];}

void IntersectLeaf(in BVHNodeGeneric Generic, in Ray Ray, inout HitInfo Intersection, inout bool Result) {
	BVHNodeLeaf Leaf = GetNodeGenericAsLeaf(Generic);

	int IterStart = Leaf.Leaf.Index;
	int IterEnd = IterStart + Leaf.Leaf.IndexCount;

	for(int Iter = IterStart; Iter < IterEnd; Iter++){
		TriangleIndexData currTriIdx = FetchIndexData(Iter);

		bool TriRes = IntersectTriangle(FetchTriangle(currTriIdx), Ray, Intersection);

		Result = Result || TriRes;
	}
	//return Result;
}

// Go with 48 for lower memory usage. I would consider 32 to be the minimum for generally all meshes
#define BVH_STACK_SIZE 64

uint GetStackIndex(in BVHNodeRecursive BNR){
	return BNR.ChildrenNodes[0];
}

uint GetStackIndex(in BVHNodeGeneric BNG){
	// A bit inefficent since I cast isntead of using the values directly but whatever...
	BVHNodeRecursive BNR = GetNodeGenericAsRecursive(BNG);
	return GetStackIndex(BNR);
}

bool TraverseBVH(in Ray IntersectionRay, inout HitInfo Intersection) {
	int HeatMap = 0;

	Ray InverseRay;

	InverseRay.Direction = 1.0f / IntersectionRay.Direction;
	InverseRay.Origin    = -IntersectionRay.Origin * InverseRay.Direction;

	BVHNodeRecursive RootNode = GetRootNode();

	uint CurrentNode = GetStackIndex(RootNode);

	if(!ValidateIntersection(IntersectNode(GetRecursiveNodeAsGeneric(RootNode), InverseRay, Intersection))){
		//imageStore(ColorOutput, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(0.0f), 1.0f));
		return false;
	} 
	else{
		HeatMap++;
	}

	bool Result = false;

	uint Stack[BVH_STACK_SIZE];
	int StackIndex = -1;

	while (true) {
		BVHNodeGeneric Children[2];

		Children[0] = GetNode(CurrentNode    );
		Children[1] = GetNode(CurrentNode + 1);

		vec2 ChildrenIntersectionDistances[2];

		ChildrenIntersectionDistances[0] = IntersectNode(Children[0], InverseRay, Intersection);
		ChildrenIntersectionDistances[1] = IntersectNode(Children[1], InverseRay, Intersection);

		bool ChildrenIntersectionSuccess[2];

		ChildrenIntersectionSuccess[0] = ValidateIntersection(ChildrenIntersectionDistances[0]);
		ChildrenIntersectionSuccess[1] = ValidateIntersection(ChildrenIntersectionDistances[1]);

		if (ChildrenIntersectionSuccess[0] && Children[0].Data[1] < 0) {
			IntersectLeaf(Children[0], IntersectionRay, Intersection, Result);
			ChildrenIntersectionSuccess[0] = false;
		}

		if (ChildrenIntersectionSuccess[1] && Children[1].Data[1] < 0) {
			IntersectLeaf(Children[1], IntersectionRay, Intersection, Result);
			ChildrenIntersectionSuccess[1] = false;
		}

		// At least one node was intersected
		if(ChildrenIntersectionSuccess[0] || ChildrenIntersectionSuccess[1]) {
			if(ChildrenIntersectionSuccess[0] && ChildrenIntersectionSuccess[1]) {
				// Both children were intersected. We need to sort by distance. Child 1 should be the one pushed on to the stack
				if(ChildrenIntersectionDistances[0].x > ChildrenIntersectionDistances[1].x){
					// Swap nodes
					BVHNodeGeneric Temp = Children[0];

					Children[0] = Children[1];
					Children[1] = Temp       ;
				}

				// Debug safety feature
				#if 0
				if(StackIndex == BVH_STACK_SIZE){
					break;
				}
				#endif

				Stack[++StackIndex] = GetStackIndex(Children[1]);

				CurrentNode = GetStackIndex(Children[0]);
			} else if(ChildrenIntersectionSuccess[0]) {
				// Child 0 was only intersected
				CurrentNode = GetStackIndex(Children[0]);
			} else {
				// Child 1 was only intersected
				CurrentNode = GetStackIndex(Children[1]);
			}
		} else {
			//TRAVERSE_STACK();
			if (StackIndex == -1) {
				 break;
			} else {
				CurrentNode = Stack[StackIndex--];
			}
		}

	}

	//imageStore(ColorOutput, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(HeatMap) / 128.0f, 1.0f));

	return Result;
}

bool TraverseMesh(in Ray R, inout HitInfo I){
	bool Res = false; 

	for(uint TriangleIndex = 0; TriangleIndex < GetTriangleCount(); TriangleIndex++){
		bool HitRes = IntersectTriangle(FetchTriangle(TriangleIndex), R, I);
		Res = Res || HitRes;
	}

	return Res;
}

/*
if one child node intersected
	current node = intersected child node
else if both child nodes interected
	reorder child nodes by distance
	push further child on to stack
	current node = closer child
else 
	current node = next node on the stack
*/

/*
function push stack node
	if stack pointer is not above stack bounds
		go to next free spot on stack by incrementing stack pointer
		write node to stack location at current pointer
	else 
		create stack overflow error

function pop stack node
	if stack pointer is not under stack bounds
		read node from stack location at current pointer
		go to last free spot on stack by decrementing stack pointer
	else
		break out of traversal loop
*/

/*

6/16/2022
New traversal algo?
The stack wil lstore the node intersection info
We only push nodes we are certain were intersected on the stack
Similair to the if-if loop in alia 2009

if intersect(parent)
	push parent node on stack
else
	return

while true
	get next node or break if empty stack
	if current triangle intersection is closer than box intersection
		get next node or break if empty stack
	if parent node
		intersect child 1
		intersect child 2
		if child 1 and child 2
			sort by intersection dist
			push both on stack
		if child 1 // (and not child 2)
			push child 1 on stack
		if child 2 // (and not child 1)
			push child 2 on stack
	else // leaf node
		intersect leaves

*/

// cause of numerical instability (line when camera flat)
vec2 IntersectNode(in BVHNodeGeneric Node, in Ray InverseRay) {
	vec3 t_node_min = Node.Node.BoundingBox.Min * InverseRay.Direction + InverseRay.Origin;
	vec3 t_node_max = Node.Node.BoundingBox.Max * InverseRay.Direction + InverseRay.Origin;

	vec3 t_min = min(t_node_min, t_node_max);
	vec3 t_max = max(t_node_min, t_node_max);

	float t_entry = max(t_min.x, max(t_min.y, t_min.z));
	float t_exit  = min(t_max.x, min(t_max.y, t_max.z));
	return vec2(t_entry, t_exit);
}

float IntersectMinimalData(in BVHNodeGeneric Node, in Ray InverseRay, in HitInfo Intersection, out bool res) {
	vec3 t_node_min = Node.Node.BoundingBox.Min * InverseRay.Direction + InverseRay.Origin;
	vec3 t_node_max = Node.Node.BoundingBox.Max * InverseRay.Direction + InverseRay.Origin;

	vec3 t_min = min(t_node_min, t_node_max);
	vec3 t_max = max(t_node_min, t_node_max);

	float t_entry = max(t_min.x, max(t_min.y, t_min.z));
	float t_exit = min(t_max.x, min(t_max.y, min(t_max.z, Intersection.Depth)));
	res = (t_entry <= t_exit && t_exit > 0.0f);
	return t_entry;
}

/*
Something I found interesting:
Before, my definition of NextNode included a float value called "depth"
When I had this, my speed was around 50-60 FPS, sometimes dipping below 50
When I removed depth, my FPS skyrocketed to 70-80

My explanation for this is that such a small difference in structure size can mess up cache sizes a lot
It is probably rounding up both to 16 bytes
*/
struct NextNode {
	ivec2 data;
};

bool TraverseBVH2(in Ray currRay, inout HitInfo intersection) {
	Ray inverseRay;

	inverseRay.Direction = 1.0f / currRay.Direction;
	inverseRay.Origin = -currRay.Origin * inverseRay.Direction;

	BVHNodeRecursive root = GetRootNode();

	bool rootRes;
	IntersectMinimalData(GetRecursiveNodeAsGeneric(root), inverseRay, intersection, rootRes);
	if (!rootRes) {
		return false;
	}
	
	bool hitResult = false;
	NextNode stack[26]; // log2(100 million vertices) = 26

	stack[0].data.x = root.ChildrenNodes[0];
	stack[0].data.y = root.ChildrenNodes[1];

	int stackIdx = 0; // next node to pop
	while (stackIdx != -1) {
		NextNode currNode = stack[stackIdx--];

		if (currNode.data.y >= 0) {
			// Read both nodes
			BVHNodeGeneric child0 = GetNode(currNode.data.x), child1 = GetNode(currNode.data.x + 1);
			bool intersect0, intersect1;
			float dist0 = IntersectMinimalData(child0, inverseRay, intersection, intersect0), dist1 = IntersectMinimalData(child1, inverseRay, intersection, intersect1);

			// rewrite as stack items for every thread
			NextNode push0, push1;
			
			push0.data.x = child0.Data[0];
			push0.data.y = child0.Data[1];

			push1.data.x = child1.Data[0];
			push1.data.y = child1.Data[1];

			if (intersect0 && intersect1) {
				if (dist0 > dist1) {
					// swapping requires 3 moves of variables, certainly slower than writing directly
					// If some nodes need to swap, we wait by 3 writes, and then proceed to 2 writes to the stack, a total of 5 effective writes
					// If we write directly, we do 2+2=4
					//NextNode temp = push0;
					//push0 = push1;
					//push1 = temp;
					stack[++stackIdx] = push1;
					stack[++stackIdx] = push0;
				}
				else {
					stack[++stackIdx] = push0;
					stack[++stackIdx] = push1;
				}
			}
			else if (intersect0) 
				stack[++stackIdx] = push0;
			else if (intersect1) 
				stack[++stackIdx] = push1;
		}
		else {
			int n = currNode.data.x - currNode.data.y;

			for (int i = currNode.data.x; i < n; i++) {
				bool currRes = IntersectTriangle(FetchTriangle(i), currRay, intersection);
				hitResult = hitResult || currRes;
			}
		}
	}

	//debugColor /= ;
	return hitResult;
}

#endif
#ifndef BVH_TRAVERSE_GLSL
#define BVH_TRAVERSE_GLSL

#include "Structure.glsl"
#include "../../geometry/Object.glsl"

struct BVHStackItem {
	int Children[2];
};

// BVH_Stack_Item BVH_Stack[64];

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

BVHNodeGeneric GetNode(in BVHSamplers BVH, uint Index) {
	BVHNodeGeneric Node;

	vec4 TextureData[2];

	Index *= 2;
	TextureData[0] = texelFetch(BVH.Nodes, int(Index    ));
	TextureData[1] = texelFetch(BVH.Nodes, int(Index + 1));

	Node.Node.BoundingBox.Min    = TextureData[0].xyz;

	Node.Node.BoundingBox.Max.x  = TextureData[0].w  ;
	Node.Node.BoundingBox.Max.yz = TextureData[1].xy ;

	Node.Data[0] = floatBitsToInt(TextureData[1].z);
	Node.Data[1] = floatBitsToInt(TextureData[1].w);

	return Node;
}

BVHNodeRecursive GetRootNode(in BVHSamplers BVH) {
	BVHNodeRecursive Root = GetNodeGenericAsRecursive(GetNode(BVH, 0));

	return Root;
}

BVHNodeGeneric GetChildNode(in BVHSamplers BVH, in BVHNodeRecursive ParentNode, in uint Index) {
	BVHNodeGeneric Child;

	Child = GetNode(BVH, ParentNode.ChildrenNodes[Index]);

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
	float t_exit  = min(t_max.x, min(t_max.y, min(t_max.z, Intersection.Depth)));
	return vec2(t_entry, t_exit);
}

// assert intersection
bool ValidateIntersection(vec2 Distances) {
	return Distances.x <= Distances.y && Distances.y > 0.0f;
}

// Go to the next avaible node on the stack if there is any
//#define TRAVERSE_STACK()  if (StackIndex == -1) {break;} else {CurrentNode = Stack[StackIndex--];}

void IntersectLeaf(in MeshSamplers M, in BVHSamplers BVH, in BVHNodeGeneric Generic, in Ray Ray, inout HitInfo Intersection, inout bool Result) {
	BVHNodeLeaf Leaf = GetNodeGenericAsLeaf(Generic);

	int IterStart = Leaf.Leaf.Index;
	int IterEnd = IterStart + Leaf.Leaf.IndexCount;

	for(int Iter = IterStart - 10; Iter < IterEnd + 10; Iter++){
		uint Index = texelFetch(BVH.Leaves, Iter).r;

		bool TriRes = IntersectTriangle(FetchTriangle(M, Index), Ray, Intersection);

		Result = Result || TriRes;
	}
	//return Result;
}

#define BVH_STACK_SIZE 64

uint GetStackIndex(in BVHNodeRecursive BNR){
	return BNR.ChildrenNodes[0];
}

uint GetStackIndex(in BVHNodeGeneric BNG){
	// A bit inefficent since I cast isntead of using the values directly but whatever...
	BVHNodeRecursive BNR = GetNodeGenericAsRecursive(BNG);
	return GetStackIndex(BNR);
}

bool TraverseBVH(in MeshSamplers M, in BVHSamplers BVH, in Ray IntersectionRay, inout HitInfo Intersection) {
	int HeatMap = 0;

	Ray InverseRay;

	InverseRay.Direction = 1.0f / IntersectionRay.Direction;
	InverseRay.Origin    = -IntersectionRay.Origin * InverseRay.Direction;

	BVHNodeRecursive RootNode = GetRootNode(BVH);

	uint CurrentNode = GetStackIndex(RootNode);

	if(!ValidateIntersection(IntersectNode(GetRecursiveNodeAsGeneric(RootNode), InverseRay, Intersection))){
		imageStore(ColorOutput, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(0.0f), 1.0f));
		return false;
	} else{
		HeatMap++;
	}

	bool Result = false;

	uint Stack[BVH_STACK_SIZE];
	int StackIndex = -1;

	while (true) {
		BVHNodeGeneric Children[2];

		Children[0] = GetNode(BVH, CurrentNode    );
		Children[1] = GetNode(BVH, CurrentNode + 1);

		vec2 ChildrenIntersectionDistances[2];

		ChildrenIntersectionDistances[0] = IntersectNode(Children[0], InverseRay, Intersection);
		ChildrenIntersectionDistances[1] = IntersectNode(Children[1], InverseRay, Intersection);

		bool ChildrenIntersectionSuccess[2];

		ChildrenIntersectionSuccess[0] = ValidateIntersection(ChildrenIntersectionDistances[0]);
		ChildrenIntersectionSuccess[1] = ValidateIntersection(ChildrenIntersectionDistances[1]);

		HeatMap += int(ChildrenIntersectionSuccess[0]);
		HeatMap += int(ChildrenIntersectionSuccess[1]);

		// Rename variable
		// #define ChildrenIntersectionSuccess PushStack

		if (ChildrenIntersectionSuccess[0] && Children[0].Data[1] < 0) {
			IntersectLeaf(M, BVH, Children[0], IntersectionRay, Intersection, Result);
			ChildrenIntersectionSuccess[0] = false;
		}

		if (ChildrenIntersectionSuccess[1] && Children[1].Data[1] < 0) {
			IntersectLeaf(M, BVH, Children[1], IntersectionRay, Intersection, Result);
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
				if(StackIndex == BVH_STACK_SIZE){
					break;
				}

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

	imageStore(ColorOutput, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(HeatMap) / 128.0f, 1.0f));

	return Result;
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

#endif
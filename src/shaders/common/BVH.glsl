#ifndef BVH_TRAVERSE_GLSL
#define BVH_TRAVERSE_GLSL

#include "Geometry.glsl"

#ifndef BVH_GLSL
#define BVH_GLSL

/*
My thoughts when planning to implement a BVH:

Fixed depth BVH: We cram everything into a single texture

How I would do this in C/++:

struct BVH_Leaf{
	// Number of triangles in this leaf
	uint32_t TriangleCount;
	// Since we are indexing the triangle buffer, we need to have a pointer to a list of indices to the indices of the vertex buffer
	uint32_t* Contents;
	// Traversal looks like this:
	// for each Triangle count:
	//    GetTriangle(IndexBuffer[Contents[Index]]).Intersect([...]);
};

struct BVH_Node {
	// Bounding box of this node
	AABB BoundingBox;
	// We either contain children nodes for traversal or we contian the leaf nodes
	// We know which one it is based on BVH depth, which can be a uniform variable
	// But beware that it does not exceed the fixed size stack
	union {
		BVH_Node* Children[2];
		BVH_Leaf* Leaf;
	};
}

Adaptation to the GPU:
First of all, replace every pointer with an index to a texture or something
This nicely allows the union in BVH_Node to not have any wasted memory
We also can support up to 4 billion or something triangles now if we use uints,
although I wish 24 bit integers were a thing since 4 billion is way too much

I might use a depth like 8 or 11 since that should nicely work in my case

I also would put everything into a texture since that allows me to use "unions"
since I can directly reinterpret data read instead of doing conversions and stuff

I'll use a buffer texture due to higher size limits

So let's look at what an AABB requires:
1. 3 floats (12 bytes) for extent[0]
2. 3 floats (12 bytes) for extent[1]
24 bytes so far, that's not divisble by the 16 byte alignment or cache stuff whatever you call it

Then the union requires:
1. 4 bytes for the first pointer
2. 4 bytes for the second point
The union in total requires 8 bytes

If we add that to our previous 24 bytes, we get 32 bytes
24 + 8 = 32

Now we divide by 16
32 / 16 = 2

We're quite lucky since we can neatly pack this into 2 vec4s

Now you maybe asking: "But how do we reinterpret floats as int (or ints as float) when we read from the texture"?
Answer: floatBitsToUint https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/floatBitsToInt.xhtml

Now comes the mpore difficult part: packing the leaf contents
Now that I tink about it, it's not a good idea to put everything into a single texture,
since the variable amount of uints for the leaf contents will not work well fdor the
vec4 16 byte alignment and it will generally become a nightmare

So instead I might have 2 textures:

uniform samplerBuffer BVH_Nodes;
uniform samplerBuffer BVH_LeafContents;

BVH_Nodes is what we have been dealing with: storing all the BVH nodes;
BVH_LeafContents is about storing the uint data about triangle indices to indices.
We can do a single uint alignment and make our lives easier

We can also put these into a single buffer and access different portions by using
glTexBufferRange to access the BVH and leaf contents seperately

My idea for memory layout of buffer

[----------|---------]
BVH nodes    leaf contents

BVH nodes (layer = depth):
[--------------------------------------------------]
layer 0   layer 1   layer 2   layer 3   layer 4 layer N

Leaf contents (leaf)
[-------------------------------------------------]
leaf 0 indeices   leaf 1 indices   leaf 2 indices

Depth is probably controlled by a uniform variable
Although this may cause a stack overflow (at which time
you should go to stackoverflow and I'm not sorry for the
bad joke), it allows for a lot more felxibility when
constructing and traversing the BVH

Also I might try to read a paper about a short stack BVH or even a stackeless BVH traversal method
*/

/*
We need to sperate the nodes into two structs since GLSL
is bad and weird and does not allow unions

I also include a generic node class and inherit from it the good old C way of
having it as a member of a derived struct
*/

/*
Traversal

First, we get the root node
For the root node, we process the children and get the intersection bounds for t
We can cull nodes based on t then
We also sort nodes by their own t
Nodes that are processed later get pushed onto a stack
A stack index is incremented
Stack size is 664
Then we iterate through the node's children

Pusedoscode:

Node root  = get root node ();

Node current node = root;



do
	ray bounds = intersect current node

	if did not intersect current node
		if stack is not empty
			go to next node on stack
		else
			exit traversal loop

	if current node is leaf
		for each triangle in leaf
			intersect triangle
	else
		intersect child[0]
		intersect child[1]

		if hit at least one child
			traverse intersected child
		if hit both children
			sort children by t
			push further child on stack
		else
			go to next node on stack

for each child in current node

Alternative (we also store info about intersection info)

if did not intersect root node
	continue

push root node on stack

while true
	if stack is empty
		break

	current node = get next node on stack

	if current node is farther than current intersection distance
		continue

	if current node is leaf
		for each triangle in leaf
			current intersection distance = intersect triangle
			intersection distance = max(intersection distance, current intersection)
	else
		intersect both children

		if did hit both children
			sort children by distance t
			push furthest child onto stack first
			push closest  child onto stack second
		else if hit single child
			push child onto stack
		else
			continue

Suggested by mad man

push root node on stack

while true
	if stack is empty
		break

	current node = get next node on stack

	intersect node

	if node aabb intersection entry > exit
		continue

	if current node is leaf
		for each triangle in leaf
			current intersection distance = intersect triangle
			intersection distance = max(intersection distance, current intersection)
	else
		intersect children
		sort children by distance
		push both children onto the stack

*/

/*
Some things I further learned from mad man:
Alia et al uses negative indices to encode leaves


*/

/*
Current traversal algorithm, based on a combanation of madman's implementation and my implementation

Stack[0] = GetRootNode();
StackIndex = 0;

while(true) {
	// Stack is empty
	if(StackIndex == -1) {
		break;
	}

	// Pop the next node from the stack
	Node CurrentNode = Stack[StackIndex--];

	AABBIntersection = Intersect(CurrentNode.BoundingBox, Ray)



}




*/

/*

next node = root node

while true

	intersect both children of next node

	intersection 1 = hit child 1 and is not occluded
	intersection 2 = hit child 2 and is not occluded

	sort children by hit distance

	if intersection 1 and child 1 is leaf
		ray triangle intersect with child 1 contents
		process child 1 = false

	if intersection 2 and child 2 is leaf
		ray triangle intersect with child 2 contents
		process child 2 = false

	if intersection 1
		if intersection 2
			push child 2 onto stack
		next node = child 1
	else if intersection 2
		next node = child 2
	else
		if stack is empty
			break
		else
			next node = get next node from stack

*/

// Leaf of a BVH
struct BVHLeaf {
	// Beginning of pointer
	int Index;
	// Size of array (also could be end of array,if I was using iterators like in C++)
	int IndexCount;
};

// Generic node wit ha bounding box
struct BVHNode {
	vec4 data[2];
};

// Node to represent a generic node

struct BVHNodeGeneric {
	AABB Box;
	ivec2 Data;
};

// Node to represent a node that refrences other nodes
struct BVHNodeRecursive {
	AABB Box;
	ivec2 ChildrenNodes;
};

// Node to represent a node that contains (or more correctly, is) a leaf
struct BVHNodeLeaf {
	AABB Box;
	ivec2 Indices;
};

/*
BVH stack

We store depth as well just in case with don't hit other nodes
*/


#endif

/*
I want the closer valid intersection to be first
*/

/*



if (!ChildrenIntersectionSuccess[0]) {
	if (!ChildrenIntersectionSuccess[1]) {
		// We missed the contents of this node; we need to go to the next one on the stack
		TRAVERSE_STACK();
	}
}
*/
#include "geometry.glsl"

struct BVHStackItem {
	int Children[2];
};

uniform samplerBuffer nodesTex;

BVHNode GetNode(int idx) {
	BVHNode node;

	idx *= 2;
	node.data[0] = texelFetch(nodesTex, idx);
	node.data[1] = texelFetch(nodesTex, idx + 1);

	return node;
}

// AABB intersection test by madmann
vec2 IntersectNode(in BVHNode node, in Ray iray, in HitInfo intersect) {
	vec3 t_node_min = node.data[0].xyz * iray.direction + iray.origin;
	vec3 t_node_max = node.data[1].xyz * iray.direction + iray.origin;

	vec3 t_min = min(t_node_min, t_node_max);
	vec3 t_max = max(t_node_min, t_node_max);

	float t_entry = max(t_min.x, max(t_min.y,     t_min.z                   ));
	float t_exit  = min(t_max.x, min(t_max.y, min(t_max.z, intersect.di.x  )));
	return vec2(t_entry, t_exit);
}

// assert intersection
bool ValidateIntersection(in vec2 distances) {
	return distances.x <= distances.y && distances.y > 0.0f;
}

// Go to the next avaible node on the stack if there is any
//#define TRAVERSE_STACK()  if (StackIndex == -1) {break;} else {CurrentNode = Stack[StackIndex--];}

void IntersectLeaf(in BVHNode leaf, in Ray ray, inout HitInfo intersection, inout bool result) {
	int i = fbs(leaf.data[0].w);
	int j = i - fbs(leaf.data[1].w);

	for(int k = i; k < j; k++){
		bool hit = IntersectTriangle(FetchTriangle(FetchIndexData(k)), ray, intersection);
		result = result || hit;
	}
}

bool IntersectLeafAny(in BVHNode leaf, in Ray ray, inout HitInfo intersection) {
	int i = fbs(leaf.data[0].w);
	int j = i - fbs(leaf.data[1].w);

	for (int k = i; k < j; k++) {
		if (IntersectTriangle(FetchTriangle(FetchIndexData(k)), ray, intersection)) {
			return true;
		}
	}

	return false;
}

#define BVH_STACK_SIZE 27

bool ClosestHit(in Ray ray, inout HitInfo intersection) {
	Ray iray;

	iray.direction = 1.0f / ray.direction;
	iray.origin    = -ray.origin * iray.direction;

	BVHNode root = GetNode(0);

	if(!ValidateIntersection(IntersectNode(root, iray, intersection))){
		return false;
	} 

	bool result = false;

	int currentNode = fbs(root.data[0].w);
	int stack[BVH_STACK_SIZE];
	int index = -1;

	while (true) {
		BVHNode child0 = GetNode(currentNode);
		BVHNode child1 = GetNode(currentNode + 1);

		vec2 distance0 = IntersectNode(child0, iray, intersection);
		vec2 distance1 = IntersectNode(child1, iray, intersection);

		bool hit0 = ValidateIntersection(distance0);
		bool hit1 = ValidateIntersection(distance1);

		// If the second node was hit but not the first, swap them to make sure all threads that just need to intersect one child don't branch 
		if (!hit0 && hit1) {
			hit0 = true;
			hit1 = false;

			BVHNode temp = child0;
			child0 = child1;
			child1 = temp;
		}

		if (hit0 && fbs(child0.data[1].w) < 0) {
			IntersectLeaf(child0, ray, intersection, result);
			hit0 = false;
		}

		if (hit1 && fbs(child1.data[1].w) < 0) {
			IntersectLeaf(child1, ray, intersection, result);
			hit1 = false;
		}

		if (hit0 && hit1) {
			if(distance0.x > distance1.x) {
				BVHNode tmpn = child0;
				child0 = child1;
				child1 = tmpn;
			}
			stack[++index] = fbs(child1.data[0].w);
			currentNode = fbs(child0.data[0].w);
		}
		else if (hit0)
			currentNode = fbs(child0.data[0].w);
		else if(hit1)
			currentNode = fbs(child1.data[0].w);
		else
			if (index == -1)
				 break;
			else 
				currentNode = stack[index--];

	}

	return result;
}

bool AnyHit(in Ray ray, inout HitInfo intersection) {
	Ray iray;

	iray.direction = 1.0f / ray.direction;
	iray.origin = -ray.origin * iray.direction;

	BVHNode root = GetNode(0);

	if (!ValidateIntersection(IntersectNode(root, iray, intersection))) {
		return false;
	}

	int currentNode = fbs(root.data[0].w);
	int stack[BVH_STACK_SIZE];
	int index = -1;

	while (true) {
		BVHNode child0 = GetNode(currentNode);
		BVHNode child1 = GetNode(currentNode + 1);

		vec2 distance0 = IntersectNode(child0, iray, intersection);
		vec2 distance1 = IntersectNode(child1, iray, intersection);

		bool hit0 = ValidateIntersection(distance0);
		bool hit1 = ValidateIntersection(distance1);

		// If the second node was hit but not the first, swap them to make sure all threads that just need to intersect one child don't branch 
		if (!hit0 && hit1) {
			hit0 = true;
			hit1 = false;

			BVHNode temp = child0;
			child0 = child1;
			child1 = temp;
		}

		if (hit0 && fbs(child0.data[1].w) < 0) {
			if (IntersectLeafAny(child0, ray, intersection)) {
				return true;
			}
			
			hit0 = false;
		}

		if (hit1 && fbs(child1.data[1].w) < 0) {
			if(IntersectLeafAny(child1, ray, intersection)) {
				return true;
			}
			hit1 = false;
		}

		if (hit0 && hit1) {
			if (distance0.x > distance1.x) {
				BVHNode tmpn = child0;
				child0 = child1;
				child1 = tmpn;
			}
			stack[++index] = fbs(child1.data[0].w);
			currentNode = fbs(child0.data[0].w);
		}
		else if (hit0)
			currentNode = fbs(child0.data[0].w);
		else if (hit1)
			currentNode = fbs(child1.data[0].w);
		else
			if (index == -1)
				break;
			else
				currentNode = stack[index--];

	}

	return false;
}

/*

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

/*

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

/*
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
*/


/*

FOR THE READER

rough code of the traversal algorithm

// 32-byte node, which is nicely aligned to 16-bytes
struct Node {
	vec3 min, max // Bounds of AABB
	int first_child_node_or_first_triangle // In memory, my nodes are organized so child nodes are consective. This way we know the second child will always be located at first_child_node_or_first_triangle + 1, making traversal a lot easier
	int num_triangles_to_intersect // A leaf must contain a subarray of triangles that need to be intersected, which can be done by keeping track of the first triangle index and number of triangles. To know what is a leaf and what is a parent node, I set the number of leaves to its negative
}

void TraverseBVH(Ray ray) {
	// Get the first node to begin traversal
	Node root = get root node();
	if(did not intersect root node)
		return

	int current_node = root.first_child_node_or_first_triangle

	int stack[26]; // The size of your stack for a binary bvh is the log2 of the number of maximum number of triangles you expect
	int stackIndex = -1
	while(true) {
		child0 = get node(current_node);
		child1 = get node(current_node + 1);


	}
}

*/


#endif
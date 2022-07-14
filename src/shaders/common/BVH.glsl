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

bool IterateAllTriangles(in Ray ray, inout HitInfo intersection) {
	bool result = false;
	for (int i = 0; i < textureSize(vertexTex) / 5; i++) {
		bool hit = IntersectTriangle(ReadPackedCompactTriangle(i), ray, intersection);
		result = result || hit;
	}
	return result;
}

void UnpackTriangleRange(in int range, out int i, out int j) {
	range = -range;
	i = range >> 4;
	j = i + (range & 15);
}

void IntersectLeaf(in BVHNode leaf, in Ray ray, inout HitInfo intersection, inout bool result) {
	int i, j;
	UnpackTriangleRange(fbs(leaf.data[0].w), i, j);

	for(int k = i; k < j; k++){
		bool hit = IntersectTriangle(ReadPackedCompactTriangle(k), ray, intersection);
		result = result || hit;
	}
}

bool IntersectLeafAny(in BVHNode leaf, in Ray ray, inout HitInfo intersection) {
	int i, j;
	UnpackTriangleRange(fbs(leaf.data[0].w), i, j);

	for (int k = i; k < j; k++) {
		if (IntersectTriangle(ReadPackedCompactTriangle(k), ray, intersection)) {
			return true;
		}
	}

	return false;
}

#define BVH_STACK_SIZE 27

bool StackTraversalClosestHit(in Ray ray, inout HitInfo intersection) {
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

		if (hit0 && fbs(child0.data[0].w) < 0) {
			IntersectLeaf(child0, ray, intersection, result);
			hit0 = false;
		}

		if (hit1 && fbs(child1.data[0].w) < 0) {
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

bool StackTraversalAnyHit(in Ray ray, inout HitInfo intersection) {
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

		if (hit0 && fbs(child0.data[0].w) < 0) {
			if (IntersectLeafAny(child0, ray, intersection)) {
				return true;
			}
			hit0 = false;
		}

		if (hit1 && fbs(child1.data[0].w) < 0) {
			if(IntersectLeafAny(child1, ray, intersection)) {
				return true;
			}
			hit1 = false;
		}

		if (hit0 && hit1) {
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

#define IsLeaf(node) (fbs(node.data[0].w) < 0)

bool IfIfClosestHit(in Ray ray, inout HitInfo intersection) {
	/*
	Alia and Laine's original model

	while ray is not terminated
		if node is not a leaf
			proceed to next node
		if node is a leaf
			test intersection with leaf

	We need to make a few modifcations before we can implement this

	We begin by saying our current node is the root node

	If our node is not a leaf, that means it is a parent with its own subtree. THis is important for proceeding to the next node
	The next node is either the closest child that was hit or the next node on the stack (if the stack is empty, then we exit traversal)

	If both children in node's subtree were hit, we proceed to the near child and push the far child onto the stack. If one was hit, we choose the hitten child. If none were hit, we go to the stack

	Once we proceed to the next node, we must check if it is a leaf. If it is a leaf, we need to test all primitives in it (assuming we are not using speculative traversal)
	Now given that our node is processed, we need to get a new node to process for the next iteration of our loop, which must come from the stack (exit traversal if it is empty)
	*/

	Ray iray;
	iray.direction = 1.0f / ray.direction;
	iray.origin = -ray.origin * iray.direction;

	BVHNode current = GetNode(0);

	BVHNode stack[32];
	int next = -1;

	bool result = false;
	while (true) {
		if (!IsLeaf(current)) {
			int index = fbs(current.data[0].w);

			BVHNode child0 = GetNode(index);
			BVHNode child1 = GetNode(index + 1);

			vec2 distance0 = IntersectNode(child0, iray, intersection);
			vec2 distance1 = IntersectNode(child1, iray, intersection);

			bool hit0 = ValidateIntersection(distance0);
			bool hit1 = ValidateIntersection(distance1);

			if (hit0 && hit1) {
				// Sort by distance 
				if (distance0.x > distance1.x) {
					BVHNode tmpn = child0;
					child0 = child1;
					child1 = tmpn;
				}

				current = child0;
				stack[++next] = child1;
			}
			else if (hit0 ^^ hit1) {
				current = (hit0 ? child0 : child1);
			}
			else {
				if (next == -1) {
					break;
				}
				current = stack[next--];
			}
		}
		if (IsLeaf(current)) {
			IntersectLeaf(current, ray, intersection, result);
			if (next == -1) {
				break;
			}
			current = stack[next--];
		}
	}

	return result;
}

const uint sentinelBit = (1 << 31);
// Shift with bew bits being zeroes
uint shiftRight(uint x) {
	x = (x >> 1);
	x = x & ~sentinelBit;
	return x;
}

layout(std430) buffer debugBuf {
	uint debug[];
};

#define RESTART_TRAIL_SHORT_STACK_SIZE 3 

/*
Short stack design:

Laine 2010 says that we basically keep a few topmost elements on the stack (typically 3), and pop them to avoid a restart

To pop an element from the stack, we check if the stack is empty. If it is, we need to restart. If it isn't, we grab the topmost node and decrement our index

The technical details:
We need to maintain two indices: a tail and a head
The tail represents the bottom of the stack, and the head represents the top
Assuming hte tail and the head are both at zero, we want to increment the head normally
If the head moves up to the stack size (ie the stack is full) we want to loop back around again, and push the tail up

A possible way to do this is to maintain two signed integers:
int tail, which maintins the bottom of our stack, bounded from [0, int_max], which is initialized to zero
int head, which maintains the location of the next item to be pushed onto the stack, bounded from [0, int_max], which is initialized to zero

If we want to add a new item to the stack, we insert it into stack[head % 3] and then increment head
Then we see if the difference between tail and head exceeds 3, in which case tail is incremented

If we want to pop an item, we decrement head. If head is equal to tail, then the stack is empty (since our next node is actually at -1, which is an invalid index), and a restart is required
Otherwsie we decrement our head, and grab the node at the new location
*/

bool RestartTrailClosestHit(in Ray ray, inout HitInfo intersection) {
	Ray iray;
	iray.direction = 1.0f / ray.direction;
	iray.origin = -ray.origin * iray.direction;

	uint trail = 0;
	uint level = sentinelBit; // most significant bit on 32-bit integers
	uint popLevel = 0; // least significant bit on 32-bit integers

	BVHNode root = GetNode(0);
	int current = fbs(root.data[0].w);
	bool result = false;

	int stack[RESTART_TRAIL_SHORT_STACK_SIZE];
	int head = 0;
	int tail = 0;

	while (true) {
		// Load our nodes from memory
		BVHNode child0 = GetNode(current);
		BVHNode child1 = GetNode(current + 1);

		// Run intersection tests
		vec2 distance0 = IntersectNode(child0, iray, intersection);
		vec2 distance1 = IntersectNode(child1, iray, intersection);
		bool hit0 = ValidateIntersection(distance0);
		bool hit1 = ValidateIntersection(distance1);

		// Go through the leaves
		if (hit0 && fbs(child0.data[0].w) < 0) {
			IntersectLeaf(child0, ray, intersection, result);
			hit0 = false;
		}

		if (hit1 && fbs(child1.data[0].w) < 0) {
			IntersectLeaf(child1, ray, intersection, result);
			hit1 = false;
		}

		// Traversal logic
		if (hit0 && hit1) {
			// Get near and far child
			int near = fbs(child0.data[0].w);
			int far = fbs(child1.data[0].w);
			if (distance0.x > distance1.x) {
				int temp = near;
				near = far;
				far = temp;
			}

			// Move onto the next level in our heirarchy
			level = shiftRight(level);

			// Use our trail to move to traversal on to the near or far child
			if ((trail & level) > 0) {
				// Move to far
				current = far;
			}
			else {
				current = near;
				// Push to short stack
				stack[head++ % RESTART_TRAIL_SHORT_STACK_SIZE] = far;
				if (head - tail == RESTART_TRAIL_SHORT_STACK_SIZE) {
					tail++;
				}
			}
		}
		else if (hit0 || hit1) {
			// Determine if we are re-entering near
			level = shiftRight(level);
			if (level != popLevel) {
				trail = trail | level;
				current = (hit0 ? fbs(child0.data[0].w) : fbs(child1.data[0].w));
			}
			else {
				// Clear any bits below level (which is useful if we pop before we get to our node)
				trail = trail & -level;
				// Clear all bits up until our first zero bit from level, and flip that bit
				trail += level;
				// Set level to the lowest on bit in trail
				uint temp = shiftRight(trail);
				level = (((temp - 1) ^ temp) + 1);
				// If we are finished with traversal, then break
				if ((trail & sentinelBit) > 0) break;
				// Mark this as the last place we started traversal
				popLevel = level;
				// Restart traversal
				if (head == tail) {
					current = fbs(root.data[0].w);
					level = sentinelBit;
				}
				else {
					current = stack[--head % RESTART_TRAIL_SHORT_STACK_SIZE];
				}
			}
		}
		else {
			// Clear any bits below level (which is useful if we pop before we get to our node)
			trail = trail & -level;
			// Clear all bits up until our first zero bit from level, and flip that bit
			trail += level;
			// Set level to the lowest on bit in trail
			uint temp = shiftRight(trail);
			level = (((temp - 1) ^ temp) + 1);
			// If we are finished with traversal, then break
			if ((trail & sentinelBit) > 0) break;
			// Mark this as the last place we started traversal
			popLevel = level;
			// Restart traversal
			if (head == tail) {
				current = fbs(root.data[0].w);
				level = sentinelBit;
			}
			else {
				current = stack[--head % RESTART_TRAIL_SHORT_STACK_SIZE];
			}
		}
	}

	return result;
}

bool RestartTrailAnyHit(in Ray ray, inout HitInfo intersection) {
	return RestartTrailClosestHit(ray, intersection); // lazy way to do it
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
#include "AABB.h"
#include <stdint.h>

AABB::AABB(void) : Min(FLT_MAX), Max(-FLT_MAX) {}

void AABB::Extend(const glm::vec3& Pos) {
	Max = glm::max(Max, Pos);
	Min = glm::min(Min, Pos);
}

float AABB::SurfaceArea(void) {
	/*
	An AABB is really just a fancy way to say "rectangular prism" (although with axis alignment restrictions, unless if it was an OBB)
	First let's define side lengths of a rectangular prism

	Equation 1:
	X = Max_X - Min_X
	Y = Max_Y - Min_Y
	Z = Max_Z - Min_Z

	Equation 1 is true as long as Max is always greater on any axis than Min, and in our case that's always true
	However in a case where the bounding box uses its default values (see constructor) then everything is broken.
	This can be fixed by the good old assert. Also, with GLM's vector math, we can just do XYZ = Max - Min

	Next we need to compute the surface area of the AABB. As we all know, area is width * length.
	Surface area is the summation of the area of all polygons on an object (at least in an object that isn't
	"continous" like a perfect sphere defined by a radius and position, but rather something like a discretized 
	set of triangles). A rectangular prism contains 6 rectangles, and the surface area is the summation of all of
	them. However, we can optimize this.

	Now there's probably a some sort of theorem for this, but I will just call it the "rectanglular prism oppisite 
	side rectangle theorm" or OSR for sort. In OSR, suppose we look at the rectangular prisim from one of the axes. 
	We will see a rectangle with a surface area which we will call A. Now we flip our directions and view the rectangle
	we now see, it will also be having a surface area of A. We can combine these two into a pair when computing surface 
	area. Instead of computing and adding the result of A twice, we could compute A once and add 2A. Another thing to note
	is that for each pair we multiply it by 2 before adding it. We can factor 2 out to reduce 2 multiplications. See equation
	2 for an optimized verion of surface area calculations and their derivations.

	Equation 2:
	SA = XY + XY + XZ + XZ + YZ + YZ
	SA = 2XY + 2XZ + 2YZ
	SA = 2(XY + XZ + YX)

	And there we have it. Fast surface area calculations. 
	*/

	glm::vec3 SideLengths = Max - Min;

	// Maybe if we stored each value in a vector before multiplying we could use vectorization

	float Area =
		SideLengths.x * SideLengths.y +
		SideLengths.x * SideLengths.z +
		SideLengths.y * SideLengths.z ;

	Area *= 2.0f;

	return Area;
}
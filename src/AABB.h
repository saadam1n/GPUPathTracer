#pragma once

#include <glm/glm.hpp>

struct AABB {
	AABB(void);
	AABB(const glm::vec3& Mi, const glm::vec3& Ma);

	void ExtendMax(const glm::vec3& Val);
	void ExtendMin(const glm::vec3& Val);

	void Extend(const glm::vec3& Pos);
	void Extend(const AABB& BBox);

	float SurfaceArea(void);
	float SurfaceAreaHalf(void);

	glm::vec3 Min;
	glm::vec3 Max;
};

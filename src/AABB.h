#pragma once

#include <glm/glm.hpp>

struct AABB {
	AABB(void);

	void Extend(const glm::vec3& Pos);
	float SurfaceArea(void);

	glm::vec3 Min;
	glm::vec3 Max;
};

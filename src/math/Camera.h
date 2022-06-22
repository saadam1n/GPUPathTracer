#pragma once

#include <glm/glm.hpp>
#include "Ray.h"
/*

In a nutshell we use this to create rays to the image plane so we can approximate the measurement equation (see section 2 in the paper "GPU-Optimized Bi-Directional Path Tracing" for more details)
This one assumes that the camera is at the origin so we handle rotation and orientation of the camera in the image plane. We handle position in the camera class

*/

using namespace glm;

class Camera {
public:
	Camera(float aspect, float fieldofview, float focus, float aperture);

	void GenerateImagePlane();

	vec3 GetPosition() const;
	void SetPosition(const vec3& Value);
	void AddPosition(const vec3& Value);

	vec3 GetRotation() const;
	void SetRotation(const vec3& Value);
	void AddRotation(const vec3& Value);

	vec3 GetDirection() const;

	void Move(float Distance);
	Ray GenRay(vec2 interpolation, float random0, float random1) const;
private: 
	vec3 position;
	vec3 direction;

	float pitch, yaw;

	float aspect_ratio, fov, focal_distance, lens_radius;
	vec3 horizontal, vertical;
	vec3 lower_left;

	vec3 u, v;

	friend class Shader;
	friend class Renderer;
};
#pragma once

#include <glm/glm.hpp>
/*

In a nutshell we use this to create rays to the image plane so we can approximate the measurement equation (see section 2 in the paper "GPU-Optimized Bi-Directional Path Tracing" for more details)
This one assumes that the camera is at the origin so we handle rotation and orientation of the camera in the image plane. We handle position in the camera class

*/
struct ImagePlane {
	glm::vec3 Corner[2][2];
};

#define IMG_PLN_PARAMS float AR, float FOV

class Camera {
public:
	void GenerateImagePlane(void);
	void SetImagePlaneParameters(IMG_PLN_PARAMS);
	void UpdateImagePlaneParameters(IMG_PLN_PARAMS);

	const ImagePlane& GetImagePlane(void) const;
private: 
	// The position. This is the starting point of all primary rays 
	glm::vec3 Position;
	// The rotation. We need to keep track of this to update our camera's orientation
	glm::vec3 Rotation;
	// The aspect ratio. We also need this so we can create the image plane
	float AspectRatio;
	// We need to know the field of view to determine how large (or more specifically, the area of the image plane) the image plane is. I, like GLM's convention, use the Y FOV instead of X. 
	float FieldOfView;
	// The image plane we want to integrate the measurement equation over
	ImagePlane ImagePlane;
};
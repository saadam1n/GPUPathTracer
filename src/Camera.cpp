#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

void Camera::GenerateImagePlane(void) {
	glm::vec2 FOV  = glm::vec2(AspectRatio * FieldOfView, FieldOfView);
	glm::vec2 Sine   = glm::sin(FOV);
	glm::vec2 Cosine = glm::cos(FOV);

	glm::vec2 ScreenDirection = glm::vec2(Sine.x * Cosine.y, Sine.y);

	float DirectionLength = glm::length(glm::vec2(ScreenDirection.x, ScreenDirection.y)) - 1.0f;

	ImagePlane.Corner[1][1].x = ScreenDirection.x;
	ImagePlane.Corner[1][1].y = ScreenDirection.y;
	ImagePlane.Corner[1][1].z = DirectionLength;

	ImagePlane.Corner[0][0]   = -ImagePlane.Corner[1][1];
	ImagePlane.Corner[0][0].z =  ImagePlane.Corner[1][1].z;

	ImagePlane.Corner[1][0]   =  ImagePlane.Corner[1][1];
	ImagePlane.Corner[1][0].x = -ImagePlane.Corner[1][0].x;

	ImagePlane.Corner[0][1]   =  ImagePlane.Corner[1][1];
	ImagePlane.Corner[0][1].y = -ImagePlane.Corner[0][1].y;

#define CNR_ACS [0][1]

	printf("(%f, %f, %f)\n", ImagePlane.Corner CNR_ACS.x, ImagePlane.Corner CNR_ACS.y, ImagePlane.Corner CNR_ACS.z);

	// TODO: fix my code later, but this will be the temporary working solution for now

	glm::mat4 ProjectionMatrix        = glm::perspective(FieldOfView, AspectRatio, 0.1f, 1.0f);
	glm::mat4 ProjectionInverseMatrix = glm::inverse    (ProjectionMatrix);

	auto GenerateDirection = [ProjectionInverseMatrix](glm::vec3 DirectionSet[2][2], int X, int Y) -> void {
		glm::vec2 ScreenCoordinates = glm::vec2(X, Y);

		glm::vec4 NDC = glm::vec4(ScreenCoordinates * 2.0f - 1.0f, 1.0f, 1.0f);

		glm::vec4 ViewSpace = ProjectionInverseMatrix * NDC;
		
		glm::vec3 Direction = glm::normalize(glm::vec3(ViewSpace.x, ViewSpace.y, ViewSpace.z) / ViewSpace.w);

		DirectionSet[Y][X] = Direction;
	};

	GenerateDirection(ImagePlane.Corner, 0, 0);
	GenerateDirection(ImagePlane.Corner, 0, 1);
	GenerateDirection(ImagePlane.Corner, 1, 0);
	GenerateDirection(ImagePlane.Corner, 1, 1);

	printf("(%f, %f, %f)\n", ImagePlane.Corner CNR_ACS.x, ImagePlane.Corner CNR_ACS.y, ImagePlane.Corner CNR_ACS.z);

}

void Camera::SetImagePlaneParameters(IMG_PLN_PARAMS) {
	AspectRatio = AR;
	FieldOfView = FOV;
}

void Camera::UpdateImagePlaneParameters(IMG_PLN_PARAMS) {
	SetImagePlaneParameters(AR, FOV);
	GenerateImagePlane();
}

const ImagePlane& Camera::GetImagePlane(void) const {
	return ImagePlane;
}
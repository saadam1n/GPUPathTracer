#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(float aspect, float fieldofview, float focus, float aperture) : direction(0.0f, 0.0f, -1.0f), position(0.0f), pitch(0.0), yaw(0.0), aspect_ratio(aspect), fov(fieldofview), focal_distance(focus), lens_radius(aperture / 2) {}

void Camera::GenerateImagePlane() {
	direction.x = cos(pitch) *  sin(yaw);
	direction.y = sin(pitch);
	direction.z = cos(pitch) * -cos(yaw);

	direction = normalize(-direction);

	float image_height = 2 * tan(fov / 2);
	float image_width = aspect_ratio * image_height;

	u = normalize(cross(glm::vec3(0, 1, 0), direction));
	v = cross(direction, u);
	
	horizontal = image_width * u * focal_distance;
	vertical = image_height * v * focal_distance;
	lower_left = -horizontal / 2.0f - vertical / 2.0f - direction * focal_distance;
}

void Camera::Move(float dist) {
	position -= direction * dist;
}

glm::vec3 Camera::GetRotation() const {
	return vec3(yaw, pitch, 0.0);
}

void Camera::SetRotation(const vec3& val) {
	yaw = val.x;
	pitch = val.y;
}

void Camera::AddRotation(const vec3& val) {
	yaw += val.x;
	pitch += val.y;
}

glm::vec3 Camera::GetPosition() const {
	return position;
}

void Camera::SetPosition(const vec3& val) {
	position = val;
}

void Camera::AddPosition(const vec3& val) {
	position += val;
}

glm::vec3 Camera::GetDirection() const {
	return direction;
}

Ray Camera::GenRay(vec2 interpolation, float random0, float random1) const {
	float phi = 2.0f * 3.141529f * random0;
	float r = sqrt(random1);

	vec2 rd = lens_radius * r * vec2(cos(phi), sin(phi));
	vec3 offset = u * rd.x + v * rd.y;

	Ray ray;
	ray.origin = position + offset;
	ray.direction = lower_left + interpolation.x * horizontal + interpolation.y * vertical - offset;
	return ray;
}
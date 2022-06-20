#version 330 core

layout(location = 0) in vec3 position;

out vec3 sampleCoords;
uniform mat4 captureView, captureProjection;

void main(){
	gl_Position = captureProjection * captureView * vec4(position,1.0f);
	sampleCoords = -position;
}
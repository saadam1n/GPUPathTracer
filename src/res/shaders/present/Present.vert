#version 330 core

layout(location = 0) in vec2 Position;

out vec2 SampleCoords;

void main(){
	gl_Position = vec4(Position, 0.0f, 1.0f);
}
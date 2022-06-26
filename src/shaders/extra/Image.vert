#version 330 core

layout(location = 0) in vec2 screenPos;

void main() {
	gl_Position = vec4(screenPos, 0.0f, 1.0f);
}
#version 330 core

out vec3 color;

uniform sampler2D image;

void main() {
	color = texelFetch(image, ivec2(gl_FragCoord.xy), 0).xyz;
}

#include "Texture.h"
#include <SOIL2.h>
#include <stdio.h>

Texture2D::Texture2D(void) : Texture2DHandle(UINT32_MAX) {

}

void Texture2D::CreateBinding(void) {
	if (Texture2DHandle == UINT32_MAX) {
		glGenTextures(1, &Texture2DHandle);
	}

	glBindTexture(GL_TEXTURE_2D, Texture2DHandle);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture2D::CreateImageBinding(uint32_t Unit) {
	glBindImageTexture(Unit, Texture2DHandle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
}

void Texture2D::FreeBinding(void) {
	glBindTexture(GL_TEXTURE_2D, 0);
}
void Texture2D::Free(void) {
	glDeleteTextures(1, &Texture2DHandle);
}

void Texture2D::LoadTexture(const char* Path) {
	int Width = 0;
	int Height = 0;
	int Channels = 0;
	unsigned char* TextureData = nullptr;

	TextureData = SOIL_load_image(Path, &Width, &Height, &Channels, SOIL_LOAD_AUTO);

	GLenum Formats[4] = {
		GL_RED,
		GL_RG,
		GL_RGB,
		GL_RGBA
	};

	GLenum Format = Formats[Channels - 1];

	LoadData(GL_RGB, Format, GL_UNSIGNED_BYTE, Width, Height, TextureData);

	SOIL_free_image_data(TextureData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
}

void Texture2D::LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data) {
	glTexImage2D(GL_TEXTURE_2D, 0, DestinationFormat, X, Y, 0, SourceFormat, SourceType, Data);
}
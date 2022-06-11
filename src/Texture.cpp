#include "Texture.h"
#include "Buffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <SOIL2.h>
#include <stdio.h>

#include <stdlib.h>

#include <iostream>
#include <map>

// perhaps I should use a proper system for taking into account already loaded textures but this will do fine, just for now
std::map<std::string, GLuint> PreloadedTextureList;

Texture::Texture(void) : TextureHandle(UINT32_MAX), RealHandle_(UINT32_MAX) {

}

GLuint Texture::GetHandle(void) {
	return TextureHandle;
}

void Texture::EnsureGeneratedHandle(void) {
	if (TextureHandle == UINT32_MAX) {
		glGenTextures(1, &RealHandle_);
		TextureHandle = RealHandle_;
	}
}

void Texture::Free(void) {
	glDeleteTextures(1, &RealHandle_);
}

void Texture2D::CreateBinding(void) {
	EnsureGeneratedHandle();

	glBindTexture(GL_TEXTURE_2D, TextureHandle);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture2D::CreateImageBinding(uint32_t Unit, GLenum Format) {
	glBindImageTexture(Unit, TextureHandle, 0, GL_FALSE, 0, GL_WRITE_ONLY, Format);
}

void Texture2D::FreeBinding(void) {
	glBindTexture(GL_TEXTURE_2D, 0);
}

bool Texture2D::AttemptPreload(const char* Path) {
	auto Iter = PreloadedTextureList.find(Path);
	if (Iter != PreloadedTextureList.end()) {
		std::cout << "Preloaded using texture handle " << Iter->second << " for path " << Path << '\n';
		TextureHandle = Iter->second;
		return true;
	}

	return false;
}

void Texture2D::LoadTexture(const char* Path) {


	std::cout << "Loading texture " << Path << '\n';

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

	PreloadedTextureList.insert({ std::string(Path), TextureHandle });
}

void Texture2D::LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data) {
	glTexImage2D(GL_TEXTURE_2D, 0, DestinationFormat, X, Y, 0, SourceFormat, SourceType, Data);
}

void TextureBuffer::CreateBinding(void) {
	EnsureGeneratedHandle();

	glBindTexture(GL_TEXTURE_BUFFER, TextureHandle);
}

void TextureBuffer::FreeBinding(void) {
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void TextureBuffer::SelectBuffer(Buffer* Buf, GLenum Format, uint32_t DataOffset, uint32_t DataBytes) {
	ReferencedBuffer = Buf;

	Offset = DataOffset;
	Bytes = DataBytes;

	glTexBufferRange(GL_TEXTURE_BUFFER, Format, ReferencedBuffer->BufferHandle, Offset, Bytes);
}

void TextureBuffer::SelectBuffer(Buffer* Buf, GLenum Format) {
	ReferencedBuffer = Buf;

	glTexBuffer(GL_TEXTURE_BUFFER, Format, ReferencedBuffer->BufferHandle);
}
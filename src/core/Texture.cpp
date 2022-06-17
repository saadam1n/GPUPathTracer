#include "Texture.h"
#include "Buffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <SOIL2.h>
#include <stdio.h>

#include <stdlib.h>

#include <iostream>
#include <map>
#include <filesystem>

// perhaps I should use a proper system for taking into account already loaded textures but this will do fine, just for now
//std::map<std::string, GLuint> PreloadedTextureList;

struct CacheLoad {
	bool Successful_;
	unsigned char* Data_;

	operator unsigned char* () {
		return Data_;
	}

	void Free() {
		if (Successful_) {
			delete[] Data_;
		}
		else {
			SOIL_free_image_data(Data_);
		}
	}
};

CacheLoad LoadFromCache(std::string Path, int& Width, int& Height, int& Channels, int force_channels) {
	for (char& c : Path) {
		if (c == '\\') {
			c = '/';
		}
	}

	constexpr bool ClearCache = false;

	CacheLoad NewLoad;

	std::string CachedPath = "cache/" + Path + ".BIN";
	FILE* CacheRead = fopen(CachedPath.c_str(), "rb");

	if (ClearCache && CacheRead) {
		fclose(CacheRead);
		remove(CachedPath.c_str());
		CacheRead = nullptr;
	}

	if (!CacheRead) {
		NewLoad.Successful_ = false;
		
		NewLoad.Data_ = SOIL_load_image(Path.c_str(), &Width, &Height, &Channels, force_channels);

		std::filesystem::create_directories(CachedPath.substr(0, CachedPath.rfind('/')));
		FILE* CreateCache = fopen(CachedPath.c_str(), "wb");

		int MetaData[] = { Width, Height, force_channels };
		fwrite(MetaData, 1, sizeof(MetaData), CreateCache);
		fwrite(NewLoad.Data_, 1, Width * Height * force_channels, CreateCache);

		fclose(CreateCache);
	}
	else {
		NewLoad.Successful_ = true;

		int MetaData[3];
		fread(MetaData, sizeof(MetaData), 1, CacheRead);

		Width = MetaData[0];
		Height = MetaData[1];
		Channels = MetaData[2];

		std::cout << Width << ' ' << Height << ' ' << Channels << '\n';

		long long Size = Width * Height * Channels;
		NewLoad.Data_ = new unsigned char[Size];
		fread(NewLoad.Data_, 1, Size, CacheRead);
		fclose(CacheRead);
	}

	return NewLoad;
}

Texture::Texture(void) : texture(UINT32_MAX) {

}

GLuint Texture::GetHandle(void) {
	return texture;
}

void Texture::EnsureGeneratedHandle(void) {
	if (texture == UINT32_MAX) {
		glGenTextures(1, &texture);
	}
}

void Texture::Free(void) {
	glDeleteTextures(1, &texture);
}

void Texture2D::CreateBinding(void) {
	EnsureGeneratedHandle();
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture::BindImageUnit(uint32_t Unit, GLenum Format) {
	glBindImageTexture(Unit, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, Format);
}

void Texture::BindTextureUnit(uint32_t unit, GLenum target) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, texture);
}

void Texture2D::FreeBinding(void) {
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::LoadTexture(const char* Path) {

	int Width = 0;
	int Height = 0;
	int Channels = 0;

	CacheLoad TextureData = LoadFromCache(Path, Width, Height, Channels, SOIL_LOAD_RGBA);

	LoadData(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, Width, Height, TextureData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture2D::LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data) {
	glTexImage2D(GL_TEXTURE_2D, 0, DestinationFormat, X, Y, 0, SourceFormat, SourceType, Data);
}

void TextureBuffer::CreateBinding(void) {
	EnsureGeneratedHandle();

	glBindTexture(GL_TEXTURE_BUFFER, texture);
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
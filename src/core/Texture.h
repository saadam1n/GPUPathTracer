#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include <SOIL2.h>
#include "OpenGL.h"
#include <string>
#include <glm/glm.hpp>
using namespace glm;

class Texture {
public:
	Texture();
	void Free();

	GLuint GetHandle();
	GLuint64 MakeBindless();

	void BindImageUnit(uint32_t Unit, GLenum Format);
	void BindTextureUnit(uint32_t unit, GLenum target);
protected:
	void EnsureGeneratedHandle();

	GLuint texture;
};

class Texture2D : public Texture {
public:
	void CreateBinding();
	void FreeBinding();

	void LoadTexture(const std::string& Path, int load = SOIL_LOAD_RGBA);
	void LoadTexture(const char* Path, int load = SOIL_LOAD_RGBA);
	void LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data);
	// Data formatting note: currently only RGB unsigned byte and RGBA float are supported for the source, and values are always stored as RGB
	void SaveData(GLenum SourceType, uint32_t X, uint32_t Y, void* Data);

	void SetColor(const vec4& color);
	void SetColor(const vec3& color);

	vec3 Sample(const vec2 texcoords) const;
private:
	uint32_t width, height;
	union {
		uint8_t* imagei;
		float* imagef;
	};
	GLenum internalFormat;
};

class Buffer;

class TextureBuffer : public Texture {
public:
	void CreateBinding();
	void FreeBinding();

	void SelectBuffer(Buffer* Buf, GLenum Format, uint32_t Offset, uint32_t Bytes);
	void SelectBuffer(Buffer* Buf, GLenum Format                                 );
private:
	Buffer* ReferencedBuffer;

	uint32_t Offset, Bytes;
};

class TextureCubemap : public Texture {
public:
	void CreateBinding();
	void FreeBinding();

	void LoadTexture(const std::string& path);

	vec3 Sample(vec3 texcoords) const;
	Texture2D& GetFace(uint32_t i);
private:
	friend class Renderer;
	Texture2D faces[6];
};
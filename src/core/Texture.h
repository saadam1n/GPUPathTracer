#pragma once

#include "OpenGL.h"

class Texture {
public:
	Texture(void);
	void Free(void);

	GLuint GetHandle(void);

	void BindImageUnit(uint32_t Unit, GLenum Format);
	void BindTextureUnit(uint32_t unit, GLenum target);
protected:
	void EnsureGeneratedHandle(void);

	GLuint TextureHandle;
	GLuint RealHandle_; // oh yeah, about that, I need to fix naming schemes 
};

class Texture2D : public Texture {
public:
	void CreateBinding(void);
	void FreeBinding(void);

	

	void LoadTexture(const char* Path);
	void LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data);
};

class Buffer;

class TextureBuffer : public Texture {
public:
	void CreateBinding(void);
	void FreeBinding(void);

	void SelectBuffer(Buffer* Buf, GLenum Format, uint32_t Offset, uint32_t Bytes);
	void SelectBuffer(Buffer* Buf, GLenum Format                                 );
private:
	Buffer* ReferencedBuffer;

	uint32_t Offset, Bytes;
};
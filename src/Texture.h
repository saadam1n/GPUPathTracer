#pragma once

#include "OpenGL.h"

class Texture {
public:
	Texture(void);
	GLuint GetHandle(void);

	void Free(void);
protected:
	void EnsureGeneratedHandle(void);

	GLuint TextureHandle;
};

class Texture2D : public Texture {
public:
	void CreateBinding(void);
	void FreeBinding(void);

	void CreateImageBinding(uint32_t Unit, GLenum Format);

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
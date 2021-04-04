#pragma once

#include "OpenGL.h"

class Texture2D {
public:
	Texture2D(void);

	void CreateBinding(void);
	void FreeBinding(void);

	void CreateImageBinding(uint32_t Unit);

	void Free(void);

	void LoadTexture(const char* Path);
	void LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data);
private:
	GLuint Texture2DHandle;
};

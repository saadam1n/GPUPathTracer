#pragma once

#include "OpenGL.h"
#include "Texture.h"

class Shader {
public:
	void CompileFiles(void) = delete;

	void CreateBinding(void);
	void FreeBinding(void);

	void Free(void);

	void LoadTexture2D(const char* Name, Texture2D& Value);
	void LoadImage2D  (const char* Name, Texture2D& Value);
protected:
	GLint GetUniformLocation(const char* Name);
	uint32_t ActivateNextFreeTextureUnit(const char* Name);

	GLuint ProgramHandle;

	uint32_t NextFreeTextureUnit;
};

class ShaderRasterization : public Shader {
public:
	void CompileFiles(const char* VertexShaderPath, const char* FragmentShaderPath);
};

class ShaderCompute : public Shader {
public:
	void CompileFile(const char* ComputeShaderPath);
};
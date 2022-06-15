#pragma once

#include "OpenGL.h"
#include "Texture.h"
#include "../math/Camera.h"
#include "Buffer.h"
#include "Scene.h"

#include <string>
#include <map>

class Shader {
public:
	void CompileFiles(void) = delete;

	void CreateBinding(void);
	void FreeBinding(void);

	void Free(void);

	void LoadTexture2D          (const char* Name, Texture2D& Value               );
	void LoadImage2D            (const char* Name, Texture2D& Value, GLenum Format = GL_RGBA16F);

	void LoadTextureBuffer      (const char* Name, TextureBuffer& Value  );

	void LoadInteger            (const char* Name, const int Value);
	void LoadFloat              (const char* Name, const float Value);
	void LoadVector3F32         (const char* Name, const glm::vec3& Value);

	void LoadCamera             (const char* Name, const Camera& Value   );

	void LoadShaderStorageBuffer(const char* Name, Buffer& Value         );
protected:
	GLint    GetUniformLocation         (const char* Name       );
	uint32_t ActivateNextFreeTextureUnit(const char* Name       );

	GLint    GetUniformLocation         (const std::string& Name);
	uint32_t ActivateNextFreeTextureUnit(const std::string& Name);

	uint32_t ActivateNextFreeTextureUnit(const int Location);

	void LoadVector3F32(GLint Location, const glm::vec3& Value);
	void LoadTexture2D(GLint Location, Texture2D& Value);

	void LoadTextureBuffer(GLint Location, TextureBuffer& Value);

	GLint       GetStructureMemberLocation(const std::string& str, const std::string& mem);

	void CreateProgramHandle(void);
	void LinkProgram(void);

	GLuint ProgramHandle;

	uint32_t NextFreeTextureUnit;
	uint32_t NextFreeBlockBinding;

	std::map <std::string, GLint> locationCache;
};

class ShaderRasterization : public Shader {
public:
	void CompileFiles(const char* VertexShaderPath, const char* FragmentShaderPath);
};

class ShaderCompute : public Shader {
public:
	void CompileFile(const char* ComputeShaderPath);
};
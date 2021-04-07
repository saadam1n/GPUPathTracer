#include "Shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>


void Shader::CreateBinding(void) {
	glUseProgram(ProgramHandle);

	NextFreeTextureUnit  = 0;
	NextFreeBlockBinding = 0;
}

void Shader::FreeBinding(void) {
	glUseProgram(0);
}

void Shader::Free(void) {
	glDeleteProgram(ProgramHandle);
}

void Shader::LoadTexture2D(const char* Name, Texture2D& Value) {
	ActivateNextFreeTextureUnit(Name);

	Value.CreateBinding();
}

void Shader::LoadImage2D(const char* Name, Texture2D& Value) {
	uint32_t TextureUnit = ActivateNextFreeTextureUnit(Name);

	Value.CreateImageBinding(TextureUnit);
}

void Shader::LoadVector3F32(const char* Name, const glm::vec3& Value) {
	LoadVector3F32(GetUniformLocation(Name), Value);
}

void Shader::LoadVector3F32(GLint Location, const glm::vec3& Value) {
	glUniform3fv(Location, 1, glm::value_ptr(Value));
}

void Shader::LoadCamera(const char* Name, const Camera& Value) {
	LoadVector3F32(GetStructureMemberLocation(Name, "Corner[0][0]"), Value.GetImagePlane().Corner[0][0]);
	LoadVector3F32(GetStructureMemberLocation(Name, "Corner[0][1]"), Value.GetImagePlane().Corner[0][1]);
	LoadVector3F32(GetStructureMemberLocation(Name, "Corner[1][0]"), Value.GetImagePlane().Corner[1][0]);
	LoadVector3F32(GetStructureMemberLocation(Name, "Corner[1][1]"), Value.GetImagePlane().Corner[1][1]);

	LoadVector3F32(GetStructureMemberLocation(Name, "Position"    ), Value.GetPosition()               );
}

void Shader::LoadShaderStorageBuffer(const char* Name, Buffer& Value) {
	uint32_t BlockBinding = NextFreeBlockBinding++;

	GLuint Location = glGetProgramResourceIndex(ProgramHandle, GL_SHADER_STORAGE_BLOCK, Name);

	glShaderStorageBlockBinding(ProgramHandle, Location, BlockBinding);

	Value.CreateBlockBinding(BUFFER_TARGET_SHADER_STORAGE, BlockBinding);
}

void Shader::LoadMesh(const char* VBuf, const char* IBuf, Mesh& Mesh) {
	LoadShaderStorageBuffer(VBuf, Mesh.VertexBuffer);
	LoadShaderStorageBuffer(IBuf, Mesh.ElementBuffer);
}

/*
End of loading code
===============================================
Beginning of shader location retrieval 
*/

std::string Shader::GetStructureMemberName(const char* Structure, const char* Member) {
	// I wish there was a easy way to reserve memory but STL sucks
	std::stringstream StringBuilder;

	StringBuilder << Structure << '.' << Member;

	return StringBuilder.str();
}

GLint Shader::GetUniformLocation(const char* Name) {
	GLint Location = glGetUniformLocation(ProgramHandle, Name);

	//assert(Location != -1);

	return Location;
}

uint32_t Shader::ActivateNextFreeTextureUnit(const char* Name) {
	glActiveTexture(GL_TEXTURE0 + NextFreeTextureUnit);
	glUniform1i(GetUniformLocation(Name), NextFreeTextureUnit);
	return NextFreeTextureUnit++;
}

GLint    Shader::GetUniformLocation(const std::string& Name) {
	return GetUniformLocation(Name.c_str());
}

uint32_t Shader::ActivateNextFreeTextureUnit(const std::string& Name) {
	return ActivateNextFreeTextureUnit(Name.c_str());
}

GLint       Shader::GetStructureMemberLocation(const char* Structure, const char* Member) {
	return GetUniformLocation(GetStructureMemberName(Structure, Member));
}

/*
End of shader location retrieval code
===============================================
Beginning of shader compilation code
*/

GLuint CompileShader(const char* ShaderPath, GLenum Type) {
	GLuint ShaderHandle = glCreateShader(Type);

	FILE* File = fopen(ShaderPath, "rb");

	fseek(File, 0, SEEK_END);
	size_t Size = ftell(File);
	rewind(File);

	char* ShaderSource = new char[Size + 1];
	ShaderSource[Size] = '\0';
	fread(ShaderSource, sizeof(char), Size, File);

	glShaderSource(ShaderHandle, 1, &ShaderSource, nullptr);
	delete[] ShaderSource;

	glCompileShader(ShaderHandle);

	GLint CompileStatus = GL_FALSE;
	glGetShaderiv(ShaderHandle, GL_COMPILE_STATUS, &CompileStatus);

	if (!CompileStatus) {
		int ShaderLogLength;
		glGetShaderiv(ShaderHandle, GL_INFO_LOG_LENGTH, &ShaderLogLength);

		char* ShaderCompileLog = new char[(uint64_t)ShaderLogLength + 1];

		ShaderCompileLog[ShaderLogLength] = '\0';

		glGetShaderInfoLog(ShaderHandle, ShaderLogLength, &ShaderLogLength, ShaderCompileLog);

		printf("Compiler error:%s\n", ShaderCompileLog);
		delete[] ShaderCompileLog;

		exit(EXIT_FAILURE);
	}
	else {
		return ShaderHandle;
	}
};


void ShaderRasterization::CompileFiles(const char* VertexShaderPath, const char* FragmentShaderPath) {

	GLuint VertexShader   = CompileShader(VertexShaderPath,   GL_VERTEX_SHADER  );
	GLuint FragmentShader = CompileShader(FragmentShaderPath, GL_FRAGMENT_SHADER);

	CreateProgramHandle();

	glAttachShader(ProgramHandle, VertexShader);
	glAttachShader(ProgramHandle, FragmentShader);

	LinkProgram();

	glDetachShader(ProgramHandle, VertexShader);
	glDetachShader(ProgramHandle, FragmentShader);

	glDeleteShader(VertexShader);
	glDeleteShader(FragmentShader);
}

void ShaderCompute::CompileFile(const char* ComputeShaderPath) {
	GLuint ComputeShader  = CompileShader(ComputeShaderPath, GL_COMPUTE_SHADER);

	CreateProgramHandle();

	glAttachShader(ProgramHandle, ComputeShader);

	LinkProgram();

	glDetachShader(ProgramHandle, ComputeShader);
	glDeleteShader(ComputeShader);
}

void Shader::LinkProgram(void) {
	glLinkProgram    (ProgramHandle);
	glValidateProgram(ProgramHandle);

	int LinkStatus;
	glGetProgramiv(ProgramHandle, GL_LINK_STATUS, &LinkStatus);

	if (!LinkStatus) {
		int LinkLogLength;
		glGetProgramiv(ProgramHandle, GL_INFO_LOG_LENGTH, &LinkLogLength);

		char* ShaderCompileLog = new char[(uint64_t)LinkLogLength + 1];

		ShaderCompileLog[LinkLogLength] = '\0';

		glGetProgramInfoLog(ProgramHandle, LinkLogLength, &LinkLogLength, ShaderCompileLog);

		printf("Linking error:%s\n", ShaderCompileLog);
		delete[] ShaderCompileLog;

		exit(EXIT_FAILURE);
	}
}

void Shader::CreateProgramHandle(void) {
	ProgramHandle = glCreateProgram();
}
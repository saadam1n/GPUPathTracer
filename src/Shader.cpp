#include "Shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sstream>
#include <fstream>
#include <regex>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <stack>

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
	LoadTexture2D(GetUniformLocation(Name), Value);
}

void Shader::LoadTextureBuffer(const char* Name, TextureBuffer& Value) {
	LoadTextureBuffer(GetUniformLocation(Name), Value);
}

void Shader::LoadTexture2D(GLint Location, Texture2D& Value) {
	ActivateNextFreeTextureUnit(Location);

	Value.CreateBinding();
}

void Shader::LoadTextureBuffer(GLint Location, TextureBuffer& Value) {
	ActivateNextFreeTextureUnit(Location);

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

void Shader::LoadMesh(const char* Buf, const char* BVH, const char* Mtrl, Mesh& Mesh) {
	//LoadShaderStorageBuffer(GetStructureMemberLocation(Mtrl, "Diffuse"), Mesh.VertexBuffer );
	//LoadShaderStorageBuffer(GetStructureMemberLocation(Mtrl, "Diffuse"), Mesh.ElementBuffer);

	//LoadVector3F32(GetStructureMemberLocation(BBox, "Max")     , Mesh.BoundingBox.Max     );
	//LoadVector3F32(GetStructureMemberLocation(BBox, "Min")     , Mesh.BoundingBox.Min     );

	LoadTextureBuffer(GetStructureMemberLocation(Buf, "Vertices"), Mesh.BufferTexture.Vertices);
	LoadTextureBuffer(GetStructureMemberLocation(Buf, "Indices" ), Mesh.BufferTexture.Indices );

	LoadTextureBuffer(GetStructureMemberLocation(BVH, "Nodes" ), Mesh.BVH.Samplers.Nodes );
	LoadTextureBuffer(GetStructureMemberLocation(BVH, "Leaves"), Mesh.BVH.Samplers.Leaves);

	LoadTexture2D(GetStructureMemberLocation(Mtrl, "Diffuse"), Mesh.Material.Diffuse);
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
	if (Location == -1) {
		printf("Warning: unable to find location for variable %s\n", Name);
	}

	return Location;
}

uint32_t Shader::ActivateNextFreeTextureUnit(const char* Name) {
	return ActivateNextFreeTextureUnit(GetUniformLocation(Name));
}

uint32_t Shader::ActivateNextFreeTextureUnit(const int Location) {
	glActiveTexture(GL_TEXTURE0 + NextFreeTextureUnit);
	glUniform1i(Location, NextFreeTextureUnit);
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

struct FileLineNumber {
	FileLineNumber(uint32_t L, const std::string F) : Line(L), File(F) {}
	uint32_t Line;
	std::string File;
};

void MakeUnixIncludeSlash(std::string Path) {
	std::replace(Path.begin(), Path.end(), '\\', '/');
}

void CollapseFilePath(std::string& Path) {
	std::string PathComponent;
	std::stack<std::string> PathComponents;

	std::istringstream InputStream(Path);
	while (std::getline(InputStream, PathComponent, '/')) {
		if (PathComponent == "..") {
			PathComponents.pop();
		} else {
			PathComponents.push(PathComponent);
		}
	}

	// Stack to vector to string

	std::vector<std::string> ReversedStack;

	while (!PathComponents.empty()) {
		ReversedStack.push_back(PathComponents.top());
		PathComponents.pop();
	}
	
	std::reverse(ReversedStack.begin(), ReversedStack.end());

	std::ostringstream StringBuilder;

	for (auto Iter = ReversedStack.begin(); Iter != ReversedStack.end(); Iter++) {
		StringBuilder << *Iter;
		if (Iter + 1 != ReversedStack.end()) {
			StringBuilder << '/';
		}
	}

	Path = StringBuilder.str();
}

// TODO: Make this non-recursive, and read the file into a string once to reduce I/O operations
std::ostringstream ParseShader(std::string& Path, std::map<uint32_t, FileLineNumber>& LineNumbers, uint32_t& GlobalLineIndex) {
	MakeUnixIncludeSlash(Path);
	std::string Folder = Path.substr(0, Path.find_last_of('/') + 1);

	printf("Shader file folder is %s\n", Folder.c_str());

	std::ostringstream ShaderParsedCode;

	std::ifstream ShaderFile(Path);
	if (!ShaderFile.is_open()) {
		printf("Invalid file path: %s\n", Path.c_str());
	}

	uint32_t LocalLineIndex = 0;

	const char* IncludeString = "#include \"";

	std::string ShaderLine;
	while (std::getline(ShaderFile, ShaderLine)) {
		if (ShaderLine.find(IncludeString) != std::string::npos) {
			std::string IncludePath = ShaderLine.substr(strlen(IncludeString));
			IncludePath.resize(IncludePath.size() - 1);

			IncludePath = Folder + IncludePath;
			CollapseFilePath(IncludePath);

			ShaderLine = ParseShader(IncludePath, LineNumbers, GlobalLineIndex).str();
		}

		LineNumbers.emplace(GlobalLineIndex++, FileLineNumber(LocalLineIndex++, Path));

		ShaderParsedCode << ShaderLine << '\n';
	}

	return ShaderParsedCode;
}

void PrintCompileLog(GLuint ShaderHandle, const std::string& ShaderSouceCC, std::map<uint32_t, FileLineNumber>& LineNumbers) {
	int ShaderLogLength;
	glGetShaderiv(ShaderHandle, GL_INFO_LOG_LENGTH, &ShaderLogLength);

	if (ShaderLogLength == 0) {
		return;
	}

	char* ShaderCompileLog = new char[(uint64_t)ShaderLogLength + 1];

	ShaderCompileLog[ShaderLogLength] = '\0';

	glGetShaderInfoLog(ShaderHandle, ShaderLogLength, &ShaderLogLength, ShaderCompileLog);

	//printf("Compiler log: \n%s", ShaderCompileLog);
	std::istringstream CompileLog(ShaderCompileLog);
	delete[] ShaderCompileLog;

	std::ostringstream CorrectedCompileLog;

	std::string LastFile;

	for (std::string Line; std::getline(CompileLog, Line);) {
		uint32_t NumberSize = (uint32_t)Line.find_first_of(')'); // or NumberOffset, NumberIndex? idk

		FileLineNumber CorrectedLineNumber = LineNumbers.find(std::stoi(Line.substr(2, NumberSize)))->second;

		if (LastFile != CorrectedLineNumber.File) {
			CorrectedCompileLog << "In file " << CorrectedLineNumber.File << ":\n";
			LastFile = CorrectedLineNumber.File;
		}

		CorrectedCompileLog << CorrectedLineNumber.Line << '\t' << Line.substr(NumberSize + 2) << '\n';
	}

	printf("%s", CorrectedCompileLog.str().c_str());

	//printf("Shader source: \n%s\n", ShaderSouceCC.c_str());
	printf("Shader source:\n");

	std::istringstream LineReader(ShaderSouceCC);

	int LineNumer = 0;
	for (std::string Line; std::getline(LineReader, Line); LineNumer++) {

		std::stringstream StringBuilder;
		StringBuilder << LineNumer << '\t' << Line << '\n';

		printf("%s", StringBuilder.str().c_str());
	}
	
}

GLuint CompileShader(const char* ShaderPath, GLenum Type) {
	GLuint ShaderHandle = glCreateShader(Type);

	uint32_t GlobalLineIndex = 0;
	std::map<uint32_t, FileLineNumber> LineMapper;

	// STL why do you have to be so garbage? I JUST WANT THE DAMN C STRING FROM A STRING STREAM BUT I HAVE TO CREATE AN ENTIRE STRING OBJECT FOR THAT LIKE BRUH
	std::ostringstream ShaderSourceCC = ParseShader(std::string(ShaderPath), LineMapper, GlobalLineIndex);
	const std::string ShaderSourceStringCC = ShaderSourceCC.str();
	const char* ShaderSource = ShaderSourceStringCC.c_str();

	glShaderSource(ShaderHandle, 1, &ShaderSource, nullptr);

	glCompileShader(ShaderHandle);
	
	PrintCompileLog(ShaderHandle, ShaderSourceStringCC, LineMapper);

	GLint CompileStatus = GL_FALSE;
	glGetShaderiv(ShaderHandle, GL_COMPILE_STATUS, &CompileStatus);

	if (!CompileStatus) {
		exit(EXIT_FAILURE);
	} else {
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
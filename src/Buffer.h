#pragma once

#include "OpenGL.h"

enum BufferTarget {
	BUFFER_TARGET_VERTEX = GL_ARRAY_BUFFER,
	BUFFER_TARGET_SHADER_STORAGE = GL_SHADER_STORAGE_BUFFER
};

class Buffer {
public:
	Buffer(void);

	void CreateBinding(BufferTarget Target);
	void FreeBinding(void);

	void UploadData(size_t Bytes, const void* Data);

	void Free(void);

	void CreateBlockBinding(BufferTarget Target, uint32_t Binding);
private:
	GLuint BufferHandle;

	BufferTarget CurrentTarget;
};
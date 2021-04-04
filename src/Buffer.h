#pragma once

#include "OpenGL.h"

enum BufferTarget {
	BUFFER_TARGET_VERTEX = GL_ARRAY_BUFFER
};

class Buffer {
public:
	Buffer(void);

	void CreateBinding(BufferTarget Target);
	void FreeBinding(void);

	void UploadData(size_t Bytes, void* Data);

	void Free(void);
private:
	GLuint BufferHandle;

	BufferTarget CurrentTarget;
};
#pragma once

#include "OpenGL.h"
#include <vector>

enum BufferTarget {
	BUFFER_TARGET_ARRAY = GL_ARRAY_BUFFER,
	BUFFER_TARGET_SHADER_STORAGE = GL_SHADER_STORAGE_BUFFER,
	BUFFER_TARGET_ATOMIC_COUNTER = GL_ATOMIC_COUNTER_BUFFER,
};

class Buffer {
public:
	Buffer(void);
	GLuint GetHandle();

	void CreateBinding(BufferTarget Target);
	void FreeBinding(void);

	void UploadData(size_t Bytes, const void* Data, GLenum usage);

	template<typename T> void UploadData(std::vector<T> vec, GLenum usage) {
		UploadData(vec.size() * sizeof(T), (const void*)vec.data(), usage);
	}

	void Free(void);

	void CreateBlockBinding(BufferTarget Target, uint32_t Binding);
	GLuint GetBlockBinding();
private:
	friend class TextureBuffer;

	GLuint BufferHandle;

	BufferTarget CurrentTarget;

	GLuint blockBinding;
};
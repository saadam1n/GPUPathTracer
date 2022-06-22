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
	void CreateBlockBinding(BufferTarget Target, uint32_t Binding, size_t offset, size_t length);
	GLuint GetBlockBinding();

	size_t GetSize();
	void DownloadBuffer(void** data, size_t* size);
	template<typename T> std::vector<T> DownloadBuffer() {
		T* ptr;
		size_t count;
		DownloadBuffer((void**)&ptr, &count);
		std::vector<T> vectorized(count);
		std::memcpy(vectorized.data(), ptr, count * sizeof(T));
		delete[] ptr;
		return vectorized;
	}
private:
	friend class TextureBuffer;

	GLuint BufferHandle;
	BufferTarget CurrentTarget;
	size_t size;
	
	GLuint blockBinding;
};
#include "Buffer.h"
#include <stdint.h>

Buffer::Buffer(void) : BufferHandle(UINT32_MAX) {}

void Buffer::CreateBinding(BufferTarget Target) {
	if (BufferHandle == UINT32_MAX) {
		glGenBuffers(1, &BufferHandle);
	}

	CurrentTarget = Target;

	glBindBuffer(CurrentTarget, BufferHandle);
}

void Buffer::FreeBinding(void) {
	glBindBuffer(CurrentTarget, 0);
}

GLuint Buffer::GetHandle() {
	return BufferHandle;
}

void Buffer::UploadData(size_t Bytes, const void* Data, GLenum access) {
	glBufferData(CurrentTarget, Bytes, Data, access);
	size = Bytes;
}

void Buffer::Free(void) {
	glDeleteBuffers(1, &BufferHandle);
}

void Buffer::CreateBlockBinding(BufferTarget Target, uint32_t Binding, size_t offset, size_t length) {
	glBindBufferRange(Target, Binding, BufferHandle, offset, length);
	blockBinding = Binding;

}

void Buffer::CreateBlockBinding(BufferTarget Target, uint32_t Binding) {
	CreateBlockBinding(Target, Binding, 0, size);
}

GLuint Buffer::GetBlockBinding() {
	return blockBinding;
}

size_t Buffer::GetSize() {
	return size;
}

void Buffer::DownloadBuffer(void** data, size_t* bytes) {
	*bytes = size;
	*data = (void*)new char[size];
	CreateBinding(BUFFER_TARGET_ARRAY);
	void* down = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
	std::memcpy(*data, down, size);
	glUnmapBuffer(GL_ARRAY_BUFFER);
}
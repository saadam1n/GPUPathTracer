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
}

void Buffer::Free(void) {
	glDeleteBuffers(1, &BufferHandle);
}

void Buffer::CreateBlockBinding(BufferTarget Target, uint32_t Binding) {
	glBindBufferBase(Target, Binding, BufferHandle);
	blockBinding = Binding;
}

GLuint Buffer::GetBlockBinding() {
	return blockBinding;
}
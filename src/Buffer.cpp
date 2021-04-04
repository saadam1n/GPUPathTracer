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

void Buffer::UploadData(size_t Bytes, void* Data) {
	glBufferData(CurrentTarget, Bytes, Data, GL_STATIC_DRAW);
}

void Buffer::Free(void) {
	glDeleteBuffers(1, &BufferHandle);
}
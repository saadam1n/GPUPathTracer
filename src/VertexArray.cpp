#include "VertexArray.h"
#include <stdint.h>

VertexArray::VertexArray(void) : VertexArrayHandle(UINT32_MAX) {}

void VertexArray::CreateBinding(void) {
	if (VertexArrayHandle == UINT32_MAX) {
		glGenVertexArrays(1, &VertexArrayHandle);
	}

	glBindVertexArray(VertexArrayHandle);
}

void VertexArray::FreeBinding(void) {
	glBindVertexArray(0);
}

void VertexArray::CreateStream(uint32_t Index, uint32_t Elements, uint32_t Stride) {
	glVertexAttribPointer(Index, Elements, GL_FLOAT, GL_FALSE, Stride, nullptr);
	glEnableVertexAttribArray(Index);
}

void VertexArray::Free(void) {
	glDeleteVertexArrays(1, &VertexArrayHandle);
}
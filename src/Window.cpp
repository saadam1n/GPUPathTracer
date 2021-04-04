#include "Window.h"

static struct GLFW_Init_Struct_T {
	GLFW_Init_Struct_T() {
		glfwInit();
	}
	~GLFW_Init_Struct_T() {
		glfwTerminate();
	}
} GLFW_Init;

void Window::Open(const char* Title, uint32_t X, uint32_t Y) {
	Width = X;
	Height = Y;

	WindowHandle = glfwCreateWindow(Width, Height, Title, nullptr, nullptr);
	glfwMakeContextCurrent(WindowHandle);
}

void Window::Close(void) {
	glfwDestroyWindow(WindowHandle);
}

bool Window::ShouldClose(void) {
	return glfwWindowShouldClose(WindowHandle);
}

void Window::Update(void) {
	glfwSwapBuffers(WindowHandle);
	glfwPollEvents();
}
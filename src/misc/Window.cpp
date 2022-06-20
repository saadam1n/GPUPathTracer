#include "Window.h"

static struct GLFW_Init_Struct_T {
	GLFW_Init_Struct_T() {
		glfwInit();
	}
	~GLFW_Init_Struct_T() {
		glfwTerminate();
	}
} GLFW_Init;

void Window::Open(const char* Title, uint32_t X, uint32_t Y, bool fullscreen) {
	Width = X;
	Height = Y;
	GLFWmonitor* monitor = nullptr;
	if (fullscreen) {
		monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		Width = mode->width;
		Height = mode->height;
	}

	WindowHandle = glfwCreateWindow(Width, Height, Title, monitor, nullptr);
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

void Window::SetInputCallback(GLFWcursorposfun MouseCallback) {
	glfwSetInputMode(WindowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(WindowHandle, MouseCallback);
}

bool Window::GetKey(uint32_t KeyCode) {
	return glfwGetKey(WindowHandle, KeyCode);
}
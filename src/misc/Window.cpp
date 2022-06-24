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
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	if (fullscreen) {
		Width = mode->width;
		Height = mode->height;
	}

	// To prevent crazzily weird FPS
	glfwSwapInterval(1);

	WindowHandle = glfwCreateWindow(Width, Height, Title, fullscreen ? monitor : nullptr, nullptr);
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
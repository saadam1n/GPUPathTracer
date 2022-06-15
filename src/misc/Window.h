#ifndef OPENGL_LIGHT_TRANSPORT_WINDOW_H
#define OPENGL_LIGHT_TRANSPORT_WINDOW_H

#include "../core/OpenGL.h"
#include <stdint.h>

class Window {
public:
	void Open(const char* Title, uint32_t Width, uint32_t Height);
	void Close(void);

	bool ShouldClose(void);
	void Update(void);

	void SetInputCallback(GLFWcursorposfun MouseCallback);

	bool GetKey(uint32_t KeyCode);

private:
	GLFWwindow* WindowHandle;

	uint32_t Width;
	uint32_t Height;
};

#endif
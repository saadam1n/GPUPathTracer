#ifndef OPENGL_LIGHT_TRANSPORT_WINDOW_H
#define OPENGL_LIGHT_TRANSPORT_WINDOW_H

#include "../core/OpenGL.h"
#include <stdint.h>
#include <chrono>
#include <ratio>

class Window {
public:
	void Open(const char* Title, uint32_t Width, uint32_t Height, bool fullscreen);
	void Close(void);

	bool ShouldClose(void);
	void Update(void);

	void SetInputCallback(GLFWcursorposfun MouseCallback);

	bool GetKey(uint32_t KeyCode);

	void SetVisibility(bool vis);

private:
	GLFWwindow* WindowHandle;

	int32_t Width;
	int32_t Height;

	friend class Renderer;
};

#endif
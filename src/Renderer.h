#ifndef OPENGL_LIGHT_TRANSPORT_RENDERER_H
#define OPENGL_LIGHT_TRANSPORT_RENDERER_H

#include "Window.h"

class Renderer {
public:
	void Init(Window* Window);
	void Free(void);

	void Begin(void);
	void End(void);
private:
	Window* WindowOutput;
};

#endif

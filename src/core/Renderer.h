#ifndef OPENGL_LIGHT_TRANSPORT_RENDERER_H
#define OPENGL_LIGHT_TRANSPORT_RENDERER_H

#include "../misc/Window.h"
#include "VertexArray.h"
#include "Buffer.h"
#include "Shader.h"
#include "Texture.h"
#include "../math/Camera.h"
#include <thread>

class Renderer {
public:
	void Initialize(Window* ptr, const char* scenePath, const std::string& env_path);
	void CleanUp();

	void RenderFrame(const Camera& camera);
	void Present();

	void ResetSamples();
	uint32_t GetNumSamples();

	void SaveScreenshot(const std::string& filename);
	void RenderReference(const Camera& camera);
private:
	uint32_t viewportWidth, viewportHeight, numPixels;
	Window* bindedWindow;

	Buffer quadBuf;
	VertexArray quadArr;
	Buffer cubeBuf;
	VertexArray cubeArr;
	ShaderRasterization present;
	Texture2D accum;

	Buffer randomState;

	ShaderCompute iterative;

	Scene scene;

	Buffer ldSamplerStateBuf;

	Buffer globalNextRayBuf;
	Buffer pixelPoolBuf;
	TextureBuffer pixelPoolTex;


	int frameCounter;
	int numSamples;
	bool running;
};

#endif

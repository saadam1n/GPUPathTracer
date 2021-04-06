#include "Window.h"
#include "Renderer.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "Camera.h"

float QuadVertices[] = {
	-1.0f,  1.0f,
	-1.0f, -1.0f,
	 1.0f, -1.0f,

	-1.0f,  1.0f,
	 1.0f, -1.0f,
	 1.0f,  1.0f
};

uint32_t Width = 1280;
uint32_t Height = 720;

int main() {

	Window Window;
	Window.Open("OpenGL Light Transport", Width, Height);

	Renderer Renderer;
	Renderer.Init(&Window);

	Buffer FullscreenQuadData;
	FullscreenQuadData.CreateBinding(BUFFER_TARGET_VERTEX);
	FullscreenQuadData.UploadData(sizeof(QuadVertices), QuadVertices);

	VertexArray FullscreenQuad;
	FullscreenQuad.CreateBinding();

	FullscreenQuad.CreateStream(0, 2, 2 * sizeof(float));

	Texture2D RenderTargetColor;
	RenderTargetColor.CreateBinding();
	RenderTargetColor.LoadData(GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, Width, Height, nullptr);

	ShaderRasterization PresentShader;
	PresentShader.CompileFiles("res/shaders/present/Present.vert", "res/shaders/present/Present.frag");

	ShaderCompute RayTraceShader;
	RayTraceShader.CompileFile("res/shaders/kernel/mega/BasicRayTracer.comp");

	Camera Camera;
	Camera.UpdateImagePlaneParameters((float)Width / (float)Height, glm::radians(45.0f));

	while (!Window.ShouldClose()) {
		Renderer.Begin();

		RayTraceShader.CreateBinding();
		RayTraceShader.LoadImage2D("ColorOutput", RenderTargetColor);
		RayTraceShader.LoadCamera ("Camera", Camera);

		glDispatchCompute(Width / 8, Height / 8, 1);

		PresentShader.CreateBinding();
		PresentShader.LoadTexture2D("ColorTexture", RenderTargetColor);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		Renderer.End();
		Window.Update();
	}

	RayTraceShader.FreeBinding();
	RayTraceShader.Free();

	PresentShader.FreeBinding();
	PresentShader.Free();
	
	RenderTargetColor.FreeBinding();
	RenderTargetColor.Free();

	FullscreenQuad.FreeBinding();
	FullscreenQuadData.FreeBinding();

	FullscreenQuad.Free();
	FullscreenQuadData.Free();

	Renderer.Free();
	Window.Close();

}
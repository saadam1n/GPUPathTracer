#include "Window.h"
#include "Renderer.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "Camera.h"
#include "TimeUtil.h"
#include "Mesh.h"

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

// Camera params 

constexpr float CameraSpeed       = 5.000f;
constexpr float CameraSensitivity = 0.001f;
glm::vec2 LastCursorPosition;

// I need class here because Intellisense is not detecting the camera type
class Camera Camera;

void MouseCallback(GLFWwindow* Window, double X, double Y) {
	glm::vec2 CurrentCursorPosition = glm::vec2(X, Y);

	glm::vec2 DeltaPosition = CurrentCursorPosition - LastCursorPosition;

	DeltaPosition.y = -DeltaPosition.y;
	//DeltaPosition.x = -DeltaPosition.x;

	DeltaPosition *= CameraSensitivity;

	Camera.AddRotation(glm::vec3(DeltaPosition, 0.0f));

	LastCursorPosition = CurrentCursorPosition;
}

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

	Camera.GenerateViewTransform();
	Camera.UpdateImagePlaneParameters((float)Width / (float)Height, glm::radians(45.0f));
	LastCursorPosition = glm::vec2(Width, Height) / 2.0f;
	Window.SetInputCallback(MouseCallback);

	Mesh Object;

	Object.LoadMesh("res/objects/Suzanne.obj");

	Timer FrameTimer;

	while (!Window.ShouldClose()) {
		FrameTimer.Begin();

		Renderer.Begin();

		if (Window.GetKey(GLFW_KEY_W)) {
			Camera.Move(CameraSpeed * (float)FrameTimer.Delta);
		} else if (Window.GetKey(GLFW_KEY_S))  {
			Camera.Move(-CameraSpeed * (float)FrameTimer.Delta);
		}
		Camera.GenerateViewTransform();
		Camera.GenerateImagePlane();

		RayTraceShader.CreateBinding();
		RayTraceShader.LoadImage2D("ColorOutput", RenderTargetColor);
		RayTraceShader.LoadCamera ("Camera", Camera);
		RayTraceShader.LoadMesh("VertexBuffer", "IndexBuffer", Object);

		glDispatchCompute(Width / 8, Height / 8, 1);

		PresentShader.CreateBinding();
		PresentShader.LoadTexture2D("ColorTexture", RenderTargetColor);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		Renderer.End();
		Window.Update();

		FrameTimer.End();

		FrameTimer.DebugTime();
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
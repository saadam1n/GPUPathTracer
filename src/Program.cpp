#include <iostream>
#include <stdio.h>
#include "misc/Window.h"
#include "core/Renderer.h"
#include "core/Buffer.h"
#include "core/VertexArray.h"
#include "core/Shader.h"
#include "core/Texture.h"
#include "math/Camera.h"
#include "misc/TimeUtil.h"
#include "core/Scene.h"



uint32_t Width = 1280;
uint32_t Height = 720;

// Camera params 

constexpr float CameraSpeed = 2000.000f * 0.001f;
constexpr float CameraSensitivity = 0.001f;
glm::vec2 LastCursorPosition;

// I need class here because Intellisense is not detecting the camera type
Camera camera;

void MouseCallback(GLFWwindow* Window, double X, double Y) {
	glm::vec2 CurrentCursorPosition = glm::vec2(X, Y);

	glm::vec2 DeltaPosition = CurrentCursorPosition - LastCursorPosition;

	DeltaPosition.y = -DeltaPosition.y;
	//DeltaPosition.x = -DeltaPosition.x;

	DeltaPosition *= CameraSensitivity;

	camera.AddRotation(glm::vec3(DeltaPosition, 0.0f));

	LastCursorPosition = CurrentCursorPosition;
}

int main(int argc, char** argv) {
	std::cout << "Working Directory: " << argv[0] << '\n';

	Window Window;
	Window.Open("OpenGL Light Transport", Width, Height);

	LastCursorPosition = glm::vec2(Width, Height) / 2.0f;
	Window.SetInputCallback(MouseCallback);

	Renderer* renderer = new Renderer;
	renderer->Initialize(&Window, "res/objects/cornellbox.obj");

	camera.UpdateImagePlaneParameters((float)Width / (float)Height, glm::radians(45.0f));
	camera.SetPosition(glm::vec3(0.0f, 0.15f, 0.5f) * 6.0f);

	Timer FrameTimer;

	while (!Window.ShouldClose()) {
		FrameTimer.Begin();

		if (Window.GetKey(GLFW_KEY_W)) {
			camera.Move(CameraSpeed * (float)FrameTimer.Delta);
		}
		else if (Window.GetKey(GLFW_KEY_S)) {
			camera.Move(-CameraSpeed * (float)FrameTimer.Delta);
		}

		camera.GenerateViewTransform();
		camera.GenerateImagePlane();


		renderer->RenderFrame(camera);
		renderer->Present();

		Window.Update();


		FrameTimer.End();
		FrameTimer.DebugTime();
	}

	renderer->CleanUp();
	Window.Close();

	delete renderer;
}
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

constexpr float CameraSpeed = 2000.000f * 0.5f;
constexpr float CameraSensitivity = 0.001f;
glm::vec2 LastCursorPosition;

// I need class here because Intellisense is not detecting the camera type
Camera camera((float)Width / Height, glm::radians(45.0f), 900.0, 2.0);

bool needResetSamples = false;

void MouseCallback(GLFWwindow* Window, double X, double Y) {
	glm::vec2 CurrentCursorPosition = glm::vec2(X, Y);

	glm::vec2 DeltaPosition = CurrentCursorPosition - LastCursorPosition;

	DeltaPosition.y = -DeltaPosition.y;
	//DeltaPosition.x = -DeltaPosition.x;

	DeltaPosition *= CameraSensitivity;

	camera.AddRotation(glm::vec3(DeltaPosition, 0.0f));

	LastCursorPosition = CurrentCursorPosition;
	needResetSamples = true;
}

int main(int argc, char** argv) {
	std::cout << "Working Directory: " << argv[0] << '\n';

	Window Window;
	Window.Open("OpenGL Light Transport", Width, Height, false);

	LastCursorPosition = glm::vec2(Width, Height) / 2.0f;
	Window.SetInputCallback(MouseCallback);

	Renderer* renderer = new Renderer;
	renderer->Initialize(&Window, "res/glTF/Sponza.gltf", "res/sky/ibl/Topanga_Forest_B_3k.hdr");
	camera.SetPosition(glm::vec3(0.0f, 0.15f, 0.5f) * 6.0f);

	Timer FrameTimer;

	while (!Window.ShouldClose()) {
		FrameTimer.Begin();

		if (Window.GetKey(GLFW_KEY_W)) {
			camera.Move(CameraSpeed * (float)FrameTimer.Delta);
			needResetSamples = true;
		}
		else if (Window.GetKey(GLFW_KEY_S)) {
			camera.Move(-CameraSpeed * (float)FrameTimer.Delta);
			needResetSamples = true;
		}

		if (needResetSamples) {
			renderer->ResetSamples();
			needResetSamples = false;
		}

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
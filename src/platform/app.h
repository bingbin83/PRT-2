#pragma once

#include <fmt/core.h>
#include <imgui.h>

#include "gl.h"
#include "util/camera.h"
#include "scene/model.h"

class Platform;

class App {
public:
	static void setup(Platform& plt);
	static App& get();


	Platform& plt;
	Shader mesh_shader;
	void render();
	Camera camera = Camera{ glm::vec3(0.0f, 0.0f, 3.0f) };
	glm::mat4 M_projection{ 1 };

private:
	App(Platform& plt);
	void clear();
	void render_imgui();
	void render_3d();

	static inline App* data = nullptr;
	bool show_demo_window = false;
	bool rotate = true;
	float depth = 1;
	ImVec4 clear_color = ImVec4(0.2f, 0.3f, 0.3f, 1.00f);
	std::unique_ptr<Model> model;
	std::unique_ptr<SkyBox> sky_box;
};
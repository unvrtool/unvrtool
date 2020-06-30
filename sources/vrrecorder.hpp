//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <opencv2/opencv.hpp>

#include "blockingqueue.hpp"
#include "shader.hpp"
#include "camera.hpp"
#include "cameratracker.hpp"
#include "videoInput.hpp"
#include "videoOutput.hpp"
#include "snapShots.hpp"
#include "gl_renderTarget.hpp"
#include "gl_base.hpp"
#include "config.hpp"
#include "vrimageformat.hpp"
#include "script.hpp"
#include "util_cv.hpp"

class VrRecorder : private GlBase
{
public:
	int channel = 0;
	std::string videopath;
	Config& c;
	bool DisableRecord = true;

	VrRecorder(Config& config);
	int Run(VrImageFormat vrFormat = VrImageFormat());

private:
	int recWidth = 1280;
	int recHeight = 720;

	VideoInput* vidIn;
	VideoOutput* vidOut;
	VideoFrame* curframe = nullptr;
	TimeCode curTimeCode;
	Script script;

	Camera cam;
	CameraTracker camTracker;
	GlRenderTarget rt, rtUv;
	Geometry geom;
	SnapShots* snapshots;
	Marker markerYpAuto = Marker(Marker::CC::BW, Marker::Shape::Circle, 12, 2, 6);
	Marker markerYpManual = Marker(Marker::CC::BW, Marker::Shape::Cross, 12, 2, 6);

	Marker markerYpSetAuto = Marker(Marker::CC::BO, Marker::Shape::Circle, 18, 6, 12);
	Marker markerYpSetManual = Marker(Marker::CC::BO, Marker::Shape::Cross, 18, 6, 12);
	Marker markerFbSet = Marker(Marker::CC::BO, Marker::Shape::Box, 36, 6, 12);
	//Marker markerFbNotSet = Marker(Marker::CC::BW, Marker::Shape::Box, 24, 2, 6);

	Shader shaderN, shaderUv, shaderN2map, shaderUv2map;
	GLFWwindow* window;

	cv::Mat GetViewUv() { return rtUv.renderImg; }

	bool trgExitScriptCamMode = false;

	bool isScriptYpChanged = false;
	bool isScriptFbChanged = false;
	int lastScriptCheckFrame = -1;

	bool scriptmode = false;
	bool showMarkers = false;
	bool pause = false;
	bool captMouse = false;
	bool firstMouse = true;
	float lastX = 0;
	float lastY = 0;
	int lastKey = 0;
	int lastMods = 0;
	cv::Size scrSize;

	void StartNormalMode();
	void StartScriptMode();
	void ExitScriptCamMode();
	void PostProcess();

	bool OpenWindow();
	void ResetView();
	void ModifyScript(bool set);
	void CheckScript(cv::Mat subFrame);

	void processInput(GLFWwindow* window);
	void framebuffer_size_callback(GLFWwindow* window, int width, int height);
	void button_callback(GLFWwindow* window, int button, int action, int mods);
	void mouse_callback(GLFWwindow* window, double xpos, double ypos);
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
};

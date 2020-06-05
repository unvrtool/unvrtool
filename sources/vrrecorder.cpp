//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <ostream>

#include <chrono>
#include <thread>

#include <algorithm>
#include <numeric>
#include <vector>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>

#include "util.hpp"
#include "vrrecorder.hpp"

#include "spheregenerator.hpp"


VrRecorder::VrRecorder(Config& config) : c(config)
{
	recWidth = c.getWidth();
	recHeight = c.getHeight();

	snapshots = new SnapShots(c);
}


bool
VrRecorder::OpenWindow()
{
	GlInit();
	window = glfwCreateWindow(scrSize.width, scrSize.height, "UnVR Tool", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}
	glfwMakeContextCurrent(window);
	glfwSetWindowUserPointer(window, this);


	glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) { static_cast<VrRecorder*>(glfwGetWindowUserPointer(w))->key_callback(w, key, scancode, action, mods); });

	glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) { static_cast<VrRecorder*>(glfwGetWindowUserPointer(w))->framebuffer_size_callback(w, width, height); });
	glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) { static_cast<VrRecorder*>(glfwGetWindowUserPointer(w))->mouse_callback(w, xpos, ypos); });
	glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) { static_cast<VrRecorder*>(glfwGetWindowUserPointer(w))->button_callback(w, button, action, mods); });
	glfwSetScrollCallback(window, [](GLFWwindow* w, double xoffset, double yoffset) { static_cast<VrRecorder*>(glfwGetWindowUserPointer(w))->scroll_callback(w, xoffset, yoffset); });

	glfwSetInputMode(window, GLFW_CURSOR, captMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return false;
	}

	glEnable(GL_DEPTH_TEST);
	shaderN.Init(false, false);
	shaderUv.Init(true, false);
	shaderN2map.Init(false, true);
	shaderUv2map.Init(true, true);
	return true;
}

bool trgExitScriptCamMode = false;

void
VrRecorder::StartScriptMode()
{
	scriptmode = true;
	showMarkers = true;
	pause = true;

	script.Load(c.loadscriptPath != "" ? c.loadscriptPath : videopath + Script::DefExt(), vidIn->fps);

	if (script.Get(0) == nullptr)
	{
		isScriptYpChanged = isScriptFbChanged = true;
		ModifyScript(true); // Make sure there is always an initial state
		isScriptYpChanged = isScriptFbChanged = false;
	}
}

void
VrRecorder::ExitScriptCamMode()
{
	scriptmode = false;
	showMarkers = false;
	if (captMouse)
	{
		captMouse = false; 
		glfwSetInputMode(window, GLFW_CURSOR, captMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}

	vidIn->SetNextFrame(0);
	pause = false;
	StartNormalMode();
}

void
VrRecorder::StartNormalMode()
{
	scriptmode = false;
	if (c.save)
	{
		//std::ostringstream os;
		//os << videopath << ".unvr.vid.mp4";
		//std::string opath(os.str());
		std::string opath = videopath + ".unvr.vid.mp4";
		vidOut->Start(c, opath, vidIn->fps, cv::Size(rt.renderWidth, rt.renderHeight));
	}
}

void 
VrRecorder::CheckScript(cv::Mat subFrame)
{
	int frameNo = curTimeCode.FrameNo;
	if (lastScriptCheckFrame != frameNo)
	{
		isScriptYpChanged = false;
		isScriptFbChanged = false;
		lastScriptCheckFrame = frameNo;
	}

	Csp last;
	auto* si = script.Get(frameNo);
	bool cur = si != nullptr;
	if (!cur)
		si = script.GetBefore(frameNo);
	if (si != nullptr)
		last = *si;

	bool ypSet = last.HasYaw();
	bool fbSet = last.HasFov();

	YawPitch yp = ypSet ? last.YP() : camTracker.curTarget;
	auto tp = ucv::Conv(sphere.CalcTexFromYawPitch(yp));

	if (showMarkers)
	{
		if (cur)
		{
			if (!ypSet) // Draw auto indicator (circle)
				markerYpSetAuto.Draw(subFrame, tp, subFrame.size(), 0.7f);
			else // Draw manual indicator (crosshairs)
				markerYpSetManual.Draw(subFrame, tp, subFrame.size(), 0.7f);

			if (fbSet)
				markerFbSet.Draw(subFrame, tp, subFrame.size(), 0.7f);
		}
		else
		{
			if (!ypSet) // Draw auto indicator (circle)
				markerYpAuto.Draw(subFrame, tp, subFrame.size(), 0.3f);
			else // Draw manual indicator (crosshairs)
				markerYpManual.Draw(subFrame, tp, subFrame.size(), 0.3f);
		}
	}
}


int
VrRecorder::Run(VrImageFormat vrFormat)
{
	vidIn = new VideoInput();
	vidOut = new VideoOutput();

	if (!vidIn->Open(videopath))
		return -1;

	std::cout << videopath << std::endl;
	std::cout << c.Print(0) << std::endl;

	if (!vrFormat.IsValid())
	{
		int confirmations = c.GetInt("AutodetectConfirmations", 1);
		vrFormat.Detect(confirmations, vidIn->cap);
		std::cout << "Detected " << vrFormat.GetFovString() << " deg " << vrFormat.GetLayOutString() << " " << vrFormat.GetGeometryString() << std::endl;
		if (!vrFormat.IsValid())
		{
			if (!vrFormat.IsLayoutSet()) std::cerr << "Unable to determine if video is L/R or U/D, aborting.." << std::endl;
			if (!vrFormat.IsFovSet()) std::cerr << "Unable to determine Fov" << std::endl;
			if (!vrFormat.IsGeomTypeSet()) std::cerr << "Unable to determine Geometry" << std::endl;
			return -1;
		}
	}
	else
		std::cout << "Processing as " << vrFormat.GetFovString() << " deg " << vrFormat.GetLayOutString() << " " << vrFormat.GetGeometryString() << std::endl;

	c.vrFormat = vrFormat;
	scrSize = cv::Size(recWidth, recHeight);
	vidIn->pause = true;
	vidIn->Start();

	cam.Init(c, vidIn->fps);
	cam.UpdateCps();
	camTracker.Init(c);
	snapshots->snapshotsPath = videopath;

	if (c.scriptcam)
		StartScriptMode();
	else
		StartNormalMode();

	if (!OpenWindow())
		return -1;

	sphere.Set(vrFormat.GeomType, vrFormat.FovX, vrFormat.FovY);
	if (vrFormat.GeomType == VrImageGeometryMapping::Type::Spherical)
		sphere.sphericalEllipseRect = vrFormat.sphericalEllipseRects[0];
	sphere.GlGenerateSphere();

	unsigned int texture1 = glGenTexture();

	shaderN.use();
	shaderN.setInt("texture1", 0);
	shaderN2map.use();
	shaderN2map.setInt("texture1", 0);

	rt.Init(shaderN2map, GlRenderTarget::Type::RGB8, recWidth, recHeight);
	rtUv.Init(shaderUv2map, GlRenderTarget::Type::Uv16, recWidth, recHeight);

	int cnt = 0;
	int cntMod = 100;
	auto t1 = std::chrono::high_resolution_clock::now();

	Shader* shader = &shaderN;

	while (!glfwWindowShouldClose(window))
	{
		if (trgExitScriptCamMode)
		{
			trgExitScriptCamMode = false;
			ExitScriptCamMode();
		}

		vidIn->pause = pause;
		curframe = vidIn->GetFrame();

		if (curframe == nullptr)
			break; // Should not happen

		if (curframe->FrameNo == -1)
		{
			if (scriptmode)
			{
				if (!pause)
					vidIn->SetNextFrame(vidIn->frameCount - 1);
				pause = true;
			}
			else
				break;
		}

		curTimeCode = TimeCode(*curframe);

		cv::Mat frame = curframe->Frame;
		cv::Rect r = vrFormat.GetSubImg(channel);
		cv::Mat subFrame = frame(r);

		if (!pause)
		{
			camTracker.Process(subFrame);

			auto* si = script.GetBefore(curTimeCode.FrameNo + 1);
			if (si && !si->IsEmpty())
				cam.Set(*si, 4); // Set target

			if (!si || !si->HasYaw())
				cam.SetTarget(camTracker.curTarget);
				//cam.SetTarget(camTracker.curTarget);

			cam.MoveCam(vidIn->SecsPerImage(), vidIn->frameSpeed);
		}

		CheckScript(subFrame);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.cols);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r.width, r.height, 0, GL_BGR, GL_UNSIGNED_BYTE, subFrame.data);

		vidIn->ReleaseFrame(curframe);
		curframe = nullptr;

		if (cnt++ % cntMod == 0)
		{
			auto t2 = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
			t1 = t2;
			std::cout << (duration / cntMod * 0.000001f) << "                 " << (1000000.0f * cntMod / duration) << " fps " << std::endl;
		}

		processInput(window);

		rt.Draw(texture1, cam, sphere);
		vidOut->Write(rt.renderImg);
		if (!scriptmode)
			snapshots->Frame(rt.renderImg, vidIn->SecsPerImage());

		rtUv.Draw(texture1, cam, sphere);

		shader->use();
		glViewport(0, 0, scrSize.width, scrSize.height);
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture1);

		glm::mat4 projection = glm::perspective(glm::radians(cam.fov()), (scrSize.width / (float)scrSize.height), 0.1f, 100.0f);
		shader->setMat4("projection", projection);
		glm::mat4 view = cam.CalcView();
		shader->setMat4("view", view);

		sphere.GlDraw();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	vidIn->Close();
	vidOut->Close();

	snapshots->CreateThumbnails();

	sphere.DeleteVo();
	glfwTerminate();

	PostProcess();
	return 0;
}

void 
VrRecorder::PostProcess()
{
	if (c.saveaudio)
	{
		auto ffmpegPath = c.GetString("ffmpegPath", "ffmpeg.exe");
		std::string vpath = videopath + ".unvr.vid.mp4";
		std::string opath = videopath + ".unvr.mp4";

		if (std::filesystem::exists(vpath))
		{
			std::ostringstream os;
			//os << " -vn -i \"" << videopath << "\" -i \"" << vpath << "\" -shortest -c copy -y \"" << opath << "\"";
			os << " -i \"" << vpath << "\" -vn -i \"" << videopath << "\" -shortest -c copy -y \"" << opath << "\"";
			std::string args = os.str();
			bool ok = util::RunProcess(ffmpegPath, args);
			if (ok)
			{
				if (!c.audiokeep && std::filesystem::exists(opath))
					std::filesystem::remove(vpath);
			}

		}
	}
}

void 
VrRecorder::ModifyScript(bool set)
{
	auto frame = curTimeCode.FrameNo;
	if (!set)
		script.Delete(frame);
	else
	{
		Csp csp;
		if (isScriptYpChanged)
			csp.YP(cam.cps.YP());
		if (isScriptFbChanged)
		{
			csp.Fov(cam.cps.Fov());
			csp.BackOff(cam.cps.BackOff());
		}
		script.Set(frame, csp);
	}
}

void 
VrRecorder::ResetView()
{
	isScriptYpChanged = isScriptFbChanged = false;
	auto* si = scriptmode ? script.GetBefore(curTimeCode.FrameNo + 1) : nullptr;
	if (si)
		cam.Set(*si, 1);
	else
		cam.UpdateCps();
}


void 
VrRecorder::processInput(GLFWwindow* window)
{
#define ONKEY(key, code) if (glfwGetKey(window, GLFW_KEY_ ## key) == GLFW_PRESS) { lastKey = GLFW_KEY_ ## key; code; }

	if (lastKey != 0 && glfwGetKey(window, lastKey) == GLFW_PRESS)
		return;
	lastKey = 0;

	if (scriptmode) //  scriptcam mode
	{
		ONKEY(ESCAPE, trgExitScriptCamMode = true;);
		ONKEY(ENTER, ModifyScript(true););
		ONKEY(DELETE, ModifyScript(false););
		ONKEY(BACKSPACE, ModifyScript(false););

		ONKEY(L, script.Load(c.loadscriptPath != "" ? c.loadscriptPath : videopath + Script::DefExt(), vidIn->fps););
		ONKEY(S, script.Save(c.savescriptPath != "" ? c.savescriptPath : videopath + Script::DefExt(), vidIn->fps););
		ONKEY(SPACE, pause = !pause; if (!pause) {
			auto frame = curTimeCode.FrameNo;
			auto * s0 = script.GetBefore(frame);
			auto * s1 = script.Get(frame);
			if (s0 == nullptr) s0 = s1;
			if (s0 != nullptr) cam.Set(*s0, 7); // Instant
			if (s1 != nullptr) cam.Set(*s1, 4); // Target
		});
	}
	else // normal mode
	{
		ONKEY(ESCAPE, vidIn->Close(); vidOut->Close(); glfwSetWindowShouldClose(window, true); );
		ONKEY(SPACE, pause = !pause;);
	}


	ONKEY(T, showMarkers = !showMarkers);
	ONKEY(C, channel = 1- channel;);
	ONKEY(M, captMouse = !captMouse; glfwSetInputMode(window, GLFW_CURSOR, captMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL););

	ONKEY(R, ResetView(););


	bool shift = (lastMods & GLFW_MOD_SHIFT) != 0;
	bool ctrl = (lastMods & GLFW_MOD_CONTROL) != 0;
	bool alt = (lastMods & GLFW_MOD_ALT) != 0;

	ONKEY(UP, vidIn->frameSpeed++; std::cout << "FS " << vidIn->frameSpeed << std::endl);
	ONKEY(DOWN, vidIn->frameSpeed--; std::cout << "FS " << vidIn->frameSpeed << std::endl);

	int skipdir = 0;
	ONKEY(Z, skipdir = -1);
	ONKEY(X, skipdir = 1);
	ONKEY(LEFT, skipdir = -1);
	ONKEY(RIGHT, skipdir = 1);
	if (skipdir != 0)
	{
		auto sec = float(skipdir * vidIn->fps);
		auto frame = float(curTimeCode.FrameNo);

		if (ctrl && shift)
		{
			ScriptItem* si = skipdir == -1 ? script.GetBefore(curTimeCode.FrameNo) : script.GetAfter(curTimeCode.FrameNo);
			if (si != nullptr)
				frame = si->Frame;
		}
		else if (alt) frame += sec * 60 * (ctrl ? 10 : 1); // 10/1 minute
		else if (ctrl) frame += sec; // 1 second
		else if (shift) frame += skipdir; // 1 frame
		else frame += sec * 10; // 10 seconds

		frame = roundf(frame);
		if (frame != curTimeCode.FrameNo)
		{
			vidIn->SetNextFrame((int)frame);
		}
	}
#undef ONKEY
}

void 
VrRecorder::framebuffer_size_callback(GLFWwindow* window, int width, int height) { scrSize = cv::Size(width, height); rtUv.SetSize(width, height); }

void
VrRecorder::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) { lastMods = mods; }

void 
VrRecorder::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) 
{
	float o = -(float)yoffset;
	float dBo = 0, dFov = 0;
	bool ctrl = (lastMods & GLFW_MOD_CONTROL) != 0;
	bool alt = (lastMods & GLFW_MOD_ALT) != 0 || (lastMods & GLFW_MOD_SHIFT) != 0;
	if (ctrl && alt)
		dBo = -(dFov = o);
	if (!ctrl && !alt)
		dBo = dFov = o;
	if (ctrl && !alt)
		dFov = 3*o;
	if (!ctrl && alt)
		dBo = 3*o;

	cam.Change(Cp::Bo, dBo, scriptmode ? 1 : 7);
	cam.Change(Cp::Fov, dFov, scriptmode ? 1 : 7);
	isScriptFbChanged = true;

}

void 
VrRecorder::button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_2)
	{
		isScriptYpChanged = isScriptFbChanged = false;
		ModifyScript(true);
	}

	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_1)
	{
		cv::Mat uvMap = GetViewUv();
		cv::Vec2w v = uvMap.at<cv::Vec2w>(cv::Point((int)lastX, (int)lastY));
		float su = v[0];
		float sv = v[1];
		if (su != 0 && sv != 0)
		{
			float tx = su / (256 * 256.0f);
			float ty = sv / (256 * 256.0f);
			auto v = sphere.Tex2Dir(tx , ty);
			auto yp = cam.CalcYawPitch(v);
			float yaw = yp.Yaw;
			float pitch = yp.Pitch;
			if ((yaw != 0 && !isnormal(yaw)) || (pitch != 0 && !isnormal(pitch)))
				return; 

			cam.manualTarget = yp;
			cam.manualTargetOverride = true;

			cam.Set(Cp::Yaw, yp.Yaw, scriptmode ? 1 : 7);
			cam.Set(Cp::Pitch, yp.Pitch, scriptmode ? 1 : 7);
			isScriptYpChanged = true;

			std::cout << yp.Yaw << " , " << yp.Pitch << std::endl;
		}
	}
}

void 
VrRecorder::mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	float x = (float)xpos, y = (float)ypos;
	if (captMouse)
	{
		if (firstMouse)
		{
			lastX = x;
			lastY = y;
			firstMouse = false;
		}

		float xoffset = x - lastX;
		float yoffset = y - lastY;

		float sensitivity = 0.1f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		cam.Change(Cp::Yaw, xoffset, scriptmode ? 1 : 7);
		cam.Change(Cp::Pitch, yoffset, scriptmode ? 1 : 7);
		isScriptYpChanged = true;

		std::cout << "Yaw " << cam.yaw() << "  Pitch " << cam.pitch() << std::endl;

		cam.calcCamera();
	}

	lastX = (float)x;
	lastY = (float)y;
}


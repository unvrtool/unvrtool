//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <opencv2/opencv.hpp>

#include "camparams.hpp"
#include "config.hpp"
#include "crossfadecontroller.hpp"

class Camera
{
	float N(float b, float l) { return l < 0 ? b/2 + l : l; }

	public:
	glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 0);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	bool manualTargetOverride = false;
	YawPitch manualTarget;

	ControllerBase* cYaw = new CrossFadeController(); // X = Yaw
	ControllerBase* cPitch = new CrossFadeController(); // Y = Pitch
	ControllerBase* cFov = new CrossFadeController();
	ControllerBase* cBackOff = new CrossFadeController();
	Csp cps;

	void UpdateCps()
	{
		cps.Yaw(cYaw->curVal()); 
		cps.Pitch(cPitch->curVal());
		cps.Fov(cFov->curVal());
		cps.BackOff(cBackOff->curVal());
	}

	ControllerBase* DoSC(Cp::Enum p, std::function<void(ControllerBase* c)> func)
	{
		if (p & Cp::Yaw) func(cYaw);
		if (p & Cp::Pitch) func(cPitch);
		if (p & Cp::Fov) func(cFov);
		if (p & Cp::Bo) func(cBackOff);
	}

	ControllerBase* GetSC(Cp::Enum p)
	{
		if (p & Cp::Yaw) return cYaw;
		if (p & Cp::Pitch) return cPitch;
		if (p & Cp::Fov) return cFov;
		if (p & Cp::Bo) return cBackOff;
		throw std::exception("Invalid controller");
	}


	void Change(Cp::Enum p, float dVal, int mode) // mode: 1: pause-override, 2: Set controller, 4: set controller-target
	{
		auto* sc = GetSC(p);
		if ((mode & 1) != 0) cps[p] = sc->LimitVal(dVal + cps[p]);
		if ((mode & 2) != 0) sc->Change(dVal, true);
		if ((mode & 4) != 0) sc->Change(dVal, false);
	}

	void Set(Cp::Enum p, float val, int mode) // mode: 1: pause-override, 2: Set controller, 4: set controller-target
	{
		auto* sc = GetSC(p);
		if ((mode & 1) != 0) cps[p] = sc->LimitVal(val);
		if ((mode & 2) != 0) sc->Set(val, true);
		if ((mode & 4) != 0) sc->Set(val, false);
	}

	void Set(Csp cps, int mode, Cp::Enum set = Cp::All)
	{
		for (Cp::Enum p : { Cp::Yaw, Cp::Pitch, Cp::Fov, Cp::Bo })
			if ((p & set) != 0 && cps.IsSet(p))
				Set(p, cps[p], mode);
	}

	float yaw() { return cps.Yaw(); }
	float pitch() { return cps.Pitch(); }
	float fov() { return cps.Fov(); }
	float backOff() { return cps.BackOff(); }

	void Init(Config& c, float fps)
	{
		cYaw->SetLimits(N(c.vrFormat.FovX, c.GetFloat("MaxYaw", -60)));
		cPitch->SetLimits(N(c.vrFormat.FovY, c.GetFloat("MaxPitch", 50)));
		cFov->SetLimits(c.GetFloat("FovMin", 1), c.GetFloat("FovMax", 89));
		cBackOff->SetLimits(c.GetFloat("BackOffMin", 0), c.GetFloat("BackOffMax", 100));

		float crossFadeSecs = c.GetFloat("CrossFadeSecs", 3);
		cYaw->Reset(fps, crossFadeSecs, 0);
		cPitch->Reset(fps, crossFadeSecs, 0);
		cFov->Reset(fps, crossFadeSecs, c.GetFloat("Fov", 65));
		cBackOff->Reset(fps, crossFadeSecs, c.GetFloat("BackOff", 45));
	}

	YawPitch CalcYawPitch(glm::vec3 dir)
	{
		dir = glm::normalize(dir);
		float pitch = -asin(dir.y);
		float yaw = -dir.x / cos(pitch);
		if (abs(yaw) > 1)
			yaw = yaw > 1 ? 1.f : -1.f;
		yaw = asin(yaw);
		return YawPitch(glm::degrees(yaw), glm::degrees(pitch));
	}

	glm::vec3 CalcDirFromYawPitch(float yaw, float pitch)
	{
		glm::vec3 dir;
		dir.x = -sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		dir.y = sin(glm::radians(-pitch));
		dir.z = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		return glm::normalize(dir);
	}

	void calcCamera()
	{
		cameraFront = CalcDirFromYawPitch(yaw(), pitch());
		cameraPos = -backOff() *0.01f * cameraFront;
	}

	glm::mat4 CalcView()
	{
		calcCamera();
		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		return view;
	}

	void SetTarget(YawPitch p)
	{
		cYaw->SetTarget(p.Yaw);
		cPitch->SetTarget(p.Pitch);
	}

	void MoveCam(float secs, int frameSpeed)
	{
		if (frameSpeed > 1 && frameSpeed < 100)
		{
			float fsecs = secs / frameSpeed;
			for (int i = 1; i < frameSpeed; i++)
				MoveCam(fsecs, 1);
		}

		cPitch->Update(secs);
		cYaw->Update(secs);
		cFov->Update(secs);
		cBackOff->Update(secs);
		UpdateCps();
	}
};


//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <opencv2/opencv.hpp>

struct YawPitch
{
	float Yaw = 0;
	float Pitch = 0;

	cv::Point2f ToPoint2f() { return cv::Point2f(Yaw, Pitch); };

	YawPitch() { }

	YawPitch(float yaw, float pitch)
	{
		Yaw = yaw;
		Pitch = pitch;
	}

	YawPitch(cv::Point2f p) : YawPitch(p.x, p.y) {}
};

namespace Cp
{
	enum Enum { None = 0, Yaw = 1, Pitch = 2, Yp = 3, Fov = 4, Bo = 8, All = 15 };
};

class Csp
{
private:
	float _yaw = 0;
	float  _pitch = 0;
	float _fov = 0;
	float _backOff = 0;
	Cp::Enum _setFlags = Cp::None;
public:
	Cp::Enum Flags() { return _setFlags; }
	void MarkSet(Cp::Enum flag) { _setFlags = Cp::Enum(_setFlags | flag); }
	bool IsSet(Cp::Enum flag) { return (_setFlags & flag) != 0; }

	float& operator[](Cp::Enum p) 
	{
		if (p & Cp::Yaw) return _yaw;
		if (p & Cp::Pitch) return _pitch;
		if (p & Cp::Fov) return _fov;
		if (p & Cp::Bo) return _backOff;
		throw std::exception("Invalid controller");
	}

	float Yaw() { return _yaw; }
	float Pitch() { return _pitch; }
	float Fov() { return _fov; }
	float BackOff() { return _backOff; }
	YawPitch YP() { return YawPitch(_yaw, _pitch); }

	bool IsEmpty() { return _setFlags == Cp::None; }
	bool HasAll() { return _setFlags == Cp::All; }
	bool HasYaw() { return IsSet(Cp::Yaw); }
	bool HasPitch() { return IsSet(Cp::Pitch); }
	bool HasFov() { return IsSet(Cp::Fov); }
	bool HasBackoff() { return IsSet(Cp::Bo); }

	void Yaw(float v) { _yaw = v; MarkSet(Cp::Yaw); }
	void Pitch(float v) { _pitch = v; MarkSet(Cp::Pitch); }
	void Fov(float v) {  _fov = v; MarkSet(Cp::Fov); }
	void BackOff(float v) {  _backOff = v; MarkSet(Cp::Bo); }
	void YP(YawPitch yp) { Yaw(yp.Yaw); Pitch(yp.Pitch); }

	void Set(Cp::Enum p, float v)
	{
		if (p == Cp::Yaw) Yaw(v);
		else if (p == Cp::Pitch) Pitch(v);
		else if (p == Cp::Fov) Fov(v);
		else if (p == Cp::Bo) BackOff(v);
		else throw std::exception("Invalid controller");
	}


	Csp() {};
	Csp(float yaw, float pitch) { Yaw(yaw); Pitch(pitch); };
	Csp(YawPitch yp) : Csp(yp.Yaw, yp.Pitch) {};
	Csp(float yaw, float pitch, float fov, float bo) { Yaw(yaw); Pitch(pitch); Fov(fov); BackOff(bo); };
	Csp(YawPitch yp, float fov, float bo) : Csp(yp.Yaw, yp.Pitch, fov, bo) { };
};

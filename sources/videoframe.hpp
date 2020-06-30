//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <string>
#include <sstream>

#include <opencv2/opencv.hpp>

struct TimeCodeHMS
{
	int Hours=0, Minutes=0, Secs=0;
	float Ms=0;

	TimeCodeHMS() {}

	TimeCodeHMS(int hours, int minutes, int secs, float ms = 0)
	{
		Hours = hours;
		Minutes = minutes;
		Secs = secs;
		Ms = ms;
	}

	double ToSecs() { return (double)Hours * 3600 + Minutes * 60 + Secs + Ms*0.001; }

	TimeCodeHMS(double secs)
	{
		Hours = (int)(secs / 3600);
		Minutes = (int)((secs - ToSecs()) / 60);
		Secs = (int)(secs - ToSecs());
		Ms = (float)(secs - ToSecs()) * 1000;
	}

	TimeCodeHMS(std::string s)
	{
		int l = s.length();
		int h = s[1] == ':' ? 1 : 2;
		Hours = std::stoi(s.substr(0, h));
		Minutes = std::stoi(s.substr(h + 1, 2));
		Secs = l < h + 5 ? 0.f : std::stoi(s.substr(h + 4, 2));
		Ms = l < h + 8 ? 0.f : (float)std::stoi(s.substr(h + 7));
	}

	std::string ToString()
	{
		std::ostringstream os;
		os << Hours << ":" << std::setfill('0') << std::setw(2) << Minutes << ":" << std::setw(2) << Secs << "." << std::setw(3) << (int)(Ms);
		return os.str();
	}
};

struct TimeCode
{
	int FrameNo = -1;
	float Fps = -1;

	TimeCode() {}
	TimeCode(const TimeCode& c) { FrameNo = c.FrameNo; Fps = c.Fps; }

	void SetTimeCode(float fps, int frameNo)
	{
		Fps = fps;
		FrameNo = frameNo;
	}

	TimeCodeHMS GetHms() { return TimeCodeHMS(FrameNo / Fps); }
};

struct VideoFrame : TimeCode
{
	cv::Mat Frame;
};
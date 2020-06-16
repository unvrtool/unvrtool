//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <queue>

#include <opencv2/opencv.hpp>

#include "util.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "camparams.hpp"
#include "vectorwindow.hpp"


class CameraTracker
{
	const int FGSize = 256;
	const int HGSize = FGSize / 2;
	float fovX, fovY;
	float N(float b, float l) { return l < 0 ? b / 2 + l : l; }
	VectorWindow<cv::Point2f> targetHistory;
	int targetHistoryLimit = 100; 

	float Limit(float limit, float val)
	{
		int s = val < 0 ? -1 : 1;
		return val * s < limit ? val : s * limit; 
	}

public:
	YawPitch curTarget;

	float centerAmp = 1;
	float XOffAmp = 1;
	float YOffAmpUp = 0.5f;
	float YOffAmpDown = 1;

	float MaxPitch = 45; // 35;
	float MaxYaw = 30;
	float minMotionThr = 40;
	bool EnableDebugTexure = false;

	bool IsInit = false;

	bool cframesReady = false;
	const int CFrames = 4;
	int gfc = 0;
	std::vector<cv::Mat> rgbReduce;
	cv::Mat diffUpscale, markUpscale;
	int MaxCenters = 20, curCenter = 0;
	std::vector<int> centersX, centersY;

	cv::Point curCenterTarget;

	cv::Mat dbgImg;

	void TargetHistoryReset(int size) { targetHistoryLimit = size;  targetHistory.Reset(targetHistoryLimit); }

	void Init(Config& c, float fps)
	{
		fovX = c.vrFormat.FovX;
		fovY = c.vrFormat.FovY;

		float targetHistoryLimitSecs = c.GetFloat("TrackAverageSecs", 4.0f);
		targetHistoryLimit = (int)(targetHistoryLimitSecs * fps + 0.5f);
		targetHistory.Reset(targetHistoryLimit);

		centerAmp = c.GetFloat("TrackCenterAmp", 1);
		XOffAmp = c.GetFloat("TrackXOffAmp", 1);
		YOffAmpUp = c.GetFloat("TrackYOffAmpUp", 1);
		YOffAmpDown = c.GetFloat("TrackYOffAmpDown", 1);

		MaxPitch = N(c.vrFormat.FovX, c.GetFloat("TrackMaxYaw", -60));
		MaxYaw = N(c.vrFormat.FovY, c.GetFloat("TrackMaxPitch", 40));

		minMotionThr = c.GetFloat("MinMotionThr", 40.0f);
		EnableDebugTexure = c.GetInt("EnableDebugTexure", 0) != 0;

		curCenterTarget = cv::Point(HGSize, HGSize);
		rgbReduce = std::vector<cv::Mat>(CFrames);
		for (int i = 0; i < MaxCenters; i++)
		{
			centersX.push_back(HGSize);
			centersY.push_back(HGSize);
		}

		IsInit = true;
	}

	cv::Mat crop2;

	void Process(cv::Mat& crop)
	{
		//auto crop = frame(cv::Rect(0, 0, frame.cols / 2, frame.rows));
		cv::resize(crop, rgbReduce[gfc], cv::Size(FGSize, FGSize), 0, 0, cv::INTER_NEAREST);

		if (gfc + 1 == CFrames)
			cframesReady = true;
		if (cframesReady)
		{
			cv::Mat rgbDiff, maxRgbDiff, maxRgbDiff2;
			for (int i = 0; i < CFrames; i++)
			{
				if (i == gfc) continue;

				cv::absdiff(rgbReduce[gfc], rgbReduce[i], rgbDiff);
				if (maxRgbDiff.cols == 0)
					maxRgbDiff2 = rgbDiff.clone();
				else
					cv::max(maxRgbDiff, rgbDiff, maxRgbDiff2);
				maxRgbDiff = maxRgbDiff2.clone();
			}
			cv::Mat diff;
			cv::cvtColor(maxRgbDiff, diff, cv::COLOR_BGR2GRAY);

			diff -= minMotionThr;
			diff *= 256;
			for (int i = 0; i < FGSize; i++)
			{
				float f = (i >= HGSize ? FGSize - i : i + 1) / (float)HGSize;
				f = sin(util::pi * f * 0.5f);
				f = pow(f, centerAmp);
				diff(cv::Rect(0, i, diff.cols, 1)) *= f;
			}

			int marg = diff.rows / 32;
			cv::Mat cropDiff = diff(cv::Rect(0, marg, diff.cols, diff.rows - 2 * marg));
			cv::Moments m = cv::moments(cropDiff);
			int x0 = 0, y0 = 0;
			if (m.m00 > 0)
			{
				x0 = (int)(0.5f + m.m10 / m.m00);
				y0 = (int)(0.5f + m.m01 / m.m00) + marg;
				if (centersX.size() < MaxCenters)
				{
					centersX.push_back(0);
					centersY.push_back(0);
				}
				centersX[curCenter] = x0;
				centersY[curCenter] = y0;
				curCenter = (curCenter + 1) % MaxCenters;
			}
			if (centersX.size() > 0)
			{
				int mx = util::median(centersX);
				int my = util::median(centersY);
				int mi = 0;
				int mval = -1;
				for (int i = 0; i < centersX.size(); i++)
				{
					int d = centersX[i] * centersX[i] + centersY[i] * centersY[i];
					if (mval < 0 || d < mval)
					{
						mval = d;
						mi = i;
					}
				}

				curCenterTarget = cv::Point(centersX[mi], centersY[mi]);

				float yaw = XOffAmp * (((float)curCenterTarget.x / FGSize -0.5f) * fovX);
				float pitch = ((float)curCenterTarget.y / FGSize - 0.5f) * fovY;
				pitch *= pitch < 0 ? YOffAmpUp : YOffAmpDown;

				curTarget = YawPitch(Limit(MaxYaw, yaw), Limit(MaxPitch, pitch));
				targetHistory.Add(curTarget.ToPoint2f());
				auto a = targetHistory.GetAverage();
				curTarget = YawPitch(a);
			}

			if (EnableDebugTexure)
			{
				cv::Mat mark = diff.clone();
				mark = 0;
				mark.at<uint8_t>(cv::Point(x0, y0)) = 128;
				//mark.at<uint8_t>(mmMaxPos) = 188;
				mark.at<uint8_t>(cv::Point(curCenterTarget.x, curCenterTarget.y)) = 255;

				cv::resize(diff, diffUpscale, crop.size(), 0, 0, cv::INTER_NEAREST);
				cv::resize(mark, markUpscale, crop.size(), 0, 0, cv::INTER_NEAREST);

				std::vector<cv::Mat> channels(3);
				cv::split(crop, channels);
				channels[1] = diffUpscale;
				channels[2] = markUpscale;
				cv::merge(channels, dbgImg);
				glPixelStorei(GL_UNPACK_ROW_LENGTH, dbgImg.cols);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dbgImg.cols, dbgImg.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, dbgImg.data);
			}
		}

		gfc = (gfc + 1) % CFrames;
	}
};

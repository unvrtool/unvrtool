//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "videoinput.hpp"

#include <opencv2/opencv.hpp>

class VrImageLayout
{
public:
	int numImgsX = 0;
	int numImgsY = 0;

	int TotalImgs() { return numImgsX * numImgsY; }
	bool IsLayoutSet() { return TotalImgs() > 0; }

	void SetLayout(int numImX, int numImY) { numImgsX = numImX; numImgsY = numImY; }
	void SetMono() { SetLayout(1, 1); }
	void SetLeftRight() { SetLayout(2, 1); }
	void SetTopBottom() { SetLayout(1, 2); }

	cv::Size GetSubImgSize(cv::Size fullSz) { return cv::Size(fullSz.width / numImgsX, fullSz.height / numImgsY); }
};

class VrImageGeometryMapping
{
protected:


	cv::Rect2f ToPixelCoords(cv::Size img, cv::Rect2f textureCoords)
	{
		auto s = textureCoords;
		float sw = img.width;
		float sh = img.height;
		cv::Rect2f r(s.x * sw, s.y * sh, s.width * sw, s.height * sh);
		return r;
	}

	cv::Rect2f ToTextureCoords(cv::Size img, cv::Rect2f pixelCoords)
	{
		cv::Rect s = pixelCoords;
		float sw = 1.0f / img.width;
		float sh = 1.0f / img.height;
		cv::Rect2f r(s.x * sw, s.y * sh, s.width * sw, s.height * sh);
		return r;
	}


public:
	inline static float DefaultStereoscopicFovX = 180.0;
	enum class Type { Unknown = 0, Flat = 1, Equirectangular = 2, Fisheye = 3, };
	std::vector<cv::Rect> subImageRects;
	std::vector<cv::Rect2f> fisheyeEllipseRects;

	VrImageGeometryMapping::Type GeomType = Type::Unknown;
	float FovX = 0;
	float FovY = 0;

	void SetGeomMapping(Type type, float fovx, float fovy = 180) { GeomType = type;  FovX = fovx; FovY = fovy; }
	void Set360(Type type = Type::Equirectangular) { SetGeomMapping(type, 360); }
	void Set180(Type type = Type::Equirectangular) { SetGeomMapping(type, 180); }
	void SetStereoscopic(Type type = Type::Flat) { FovX = DefaultStereoscopicFovX; FovY = FovX * 9 / 16; GeomType = type; }

	bool IsGeomTypeSet() { return GeomType != Type::Unknown; }
	bool IsFovSet() { return FovX >= 0 && FovY >= 0; }
	bool IsGeomMappingSet() { return IsGeomTypeSet() && IsFovSet(); }
};


class VrImageFormat : public VrImageLayout, public VrImageGeometryMapping
{
	bool CheckStereo(cv::Mat img1, cv::Mat img2, bool horizontal);

public:
	cv::Mat lastFrameAnalyzed;
	void Detect(int level, std::function<cv::Mat(int, int)> getFrame);
	void Detect(int level, VideoInput* cap);

	bool IsValid() { return IsLayoutSet() && IsGeomMappingSet(); }

	cv::Rect GetSubImg(cv::Size fullSz, int imageNo = 0)
	{
		imageNo = imageNo % TotalImgs();
		cv::Size sz = GetSubImgSize(fullSz);
		int x = imageNo % numImgsX;
		int y = imageNo / numImgsX;
		return cv::Rect(cv::Point2i(x * sz.width, y * sz.height), sz);
	}

	void CheckSubImgInit(VideoInput* vidIn)
	{
		cv::Size fullsize(vidIn->width, vidIn->height);
		if (subImageRects.size() == 0)
		{
			subImageRects.push_back(GetSubImg(fullsize, 0));
			subImageRects.push_back(GetSubImg(fullsize, 1));
		}
		if (GeomType == Type::Fisheye)
		{
			if (fisheyeEllipseRects.size() == 0)
				Detect(0, vidIn);
			if (fisheyeEllipseRects.size() == 0)
				throw std::runtime_error("Unable to detect fisheye params");
		}
	}

	cv::Rect GetSubImg(int imageNo = 0)
	{
		return (subImageRects[imageNo]);
	}

	std::string GetFovString()
	{
		std::ostringstream os;
		if (FovX <= 0 || FovY <= 0) return "No Fov";
		os << FovX << "x" << FovY;
		return os.str();
	}

	std::string GetLayOutString()
	{
		switch (TotalImgs())
		{
		case 0: return "Unknown";
		case 1: return "Mono";
		case 2: return numImgsX == 2 ? "LR" : "TB";
		default: return "Unknown multi-image";
		};
	}

	void SaveDebugInputImages(const std::string& videopath, cv::Mat* debugframe, cv::Mat* projectedFrame);
	cv::Mat DrawDebugInputImage(cv::Mat frame);

	std::string GetGeometryString()
	{
		switch (GeomType)
		{
		case Type::Unknown: return "Unknown";
		case Type::Flat: return "Flat";
		case Type::Equirectangular: return "Equirectangular";
		case Type::Fisheye: return "Fisheye";
		default: return "Invalid enum";
		}
	}

	// lr:180:fisheye or tb:360:equirectangular
	static VrImageFormat Parse(std::string s)
	{
		for (auto& c : s)
			if (c >= 'A' && c <= 'Z')
				c = c - ('A' - 'a');

		VrImageFormat v;
		int f1 = s.find(":");
		if (f1 < 0) throw std::exception("Invalid VrImageFormat layout");

		int f2 = s.find(":", f1 + 1);
		if (f2 < 0) throw std::exception("Invalid VrImageFormat layout");

		auto k = s.substr(0, f1);
		if (k == "lr")
			v.SetLeftRight();
		else if (k == "tb")
			v.SetTopBottom();
		else if (k == "mono")
			v.SetMono();
		else throw std::exception("Unknown layout");

		k = s.substr(f1+1, f2-f1-1);
		if (k == "180" || k == "")
			v.Set180();
		else if (k == "360")
			v.Set360();
		else throw std::exception("Unknown FOV");

		k = s.substr(f2+1);
		if (k == "er" || k == "equirectangular")
			v.GeomType = Type::Equirectangular;
		else if (k == "flat")
			v.SetStereoscopic();
		else if (k == "fisheye" || k == "spherical")
			v.GeomType = Type::Fisheye;
		else throw std::exception("Unknown projection");


		return v;
	}
};

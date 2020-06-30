//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "_headers_std_cv_ogl.hpp"
#include "util.hpp"

class ucv
{
public:
	static cv::Point2f Conv(glm::vec2 v) { return cv::Point2f(v.x, v.y); }

	static void Ensure(cv::Mat& mat, int rows, int cols, int type)
	{
		if (mat.cols != cols || mat.rows != rows || mat.type() != type)
			mat = cv::Mat(rows, cols, type);
	}

	static cv::Rect fitEllipseToRect(cv::RotatedRect box)
	{
		double r = util::rad(box.angle);
		cv::Mat_<double> t(2, 2);
		cv::Vec2d v;

		t.at<double>(0, 0) = t.at<double>(1, 1) = cos(r);
		t.at<double>(cv::Point(1, 0)) = -sin(r);
		t.at<double>(cv::Point(0, 1)) = sin(r);

		MinMax mx, my;
		for (float deg = 0; deg < 360; deg++)
		{
			r = util::rad(deg);
			double x = 0.5f * box.size.width * cos(r);
			double y = 0.5f * box.size.height * sin(r);
			v = cv::Vec2d(x, y);
			auto d = cv::Mat(t * v);
			x = d.at<double>(0, 0);
			y = d.at<double>(1, 0);
			x += box.center.x;
			y += box.center.y;
			mx.Add(x);
			my.Add(y);
		}
		int x0 = (int)(mx.Min + 0.5f);
		int x1 = (int)(mx.Max + 0.5f);
		int y0 = (int)(my.Min + 0.5f);
		int y1 = (int)(my.Max + 0.5f);
		return cv::Rect(x0, y0, x1 - x0, y1 - y0);
	}

	static void TestfitEllipseToRect()
	{
		for (int i = 0; i < 360; i++)
		{
			cv::Mat_<uint8_t> im(1000, 1000);
			im = (uint8_t)0;
			cv::RotatedRect rr(cv::Point2f(500, 500), cv::Size(400, 200), i);
			cv::ellipse(im, rr, cv::Scalar(255), cv::LineTypes::FILLED);

			std::vector<std::vector<cv::Point>> contours;
			cv::findContours(im, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
			for (size_t i = 0; i < contours.size(); i++)
			{
				auto contour = contours[i];
				if (contour.size() < 10)
					continue;
				cv::Mat pointsf;
				cv::Mat(contour).convertTo(pointsf, CV_32F);
				cv::RotatedRect box = fitEllipse(pointsf);
				cv::Rect r = fitEllipseToRect(box);
				cv::rectangle(im, r, 200, 1);
			}

			cv::imshow("e", im);
			cv::waitKey(100);
		}

	}

};

struct Marker
{
	enum class CC { Preset=-1, BW=0, YB = 1, BO = 2 };
	enum class Shape { Circle, Box, Cross };
	const int ccRgb[3 * 6]{
	 0,0,0,  255,255,255,
	 255,0,150,  0,255,150,
	 255,150,0,  0,175, 255,
	};

	CC cc;
	Shape shape = Shape::Circle;
	cv::Scalar color1, color2;
	int size1=3, size2 = 0;
	int radius = 10;
	float alpha = 0.5f;

	void SetColor(CC c)
	{
		if (c == CC::Preset) return;
		int o = ((int)c) * 6;
		color2 = cv::Scalar(ccRgb[o + 0], ccRgb[o + 1], ccRgb[o + 2]);
		color1 = cv::Scalar(ccRgb[o + 3], ccRgb[o + 4], ccRgb[o + 5]);
	}

	Marker(CC color, Shape s, int r, int thickness1, int thickness2 = 0)
	{
		cc = color;
		shape = s;
		radius = r;
		size1 = thickness1;
		size2 = thickness2;
		if (size2 > 0 && size2 < size1)
		{
			size1 = thickness2;
			size2 = thickness1;
		}
		SetColor(cc);
	}

	void Draw(cv::Mat& m, cv::Point c, float alpha=-1)
	{
		try
		{
			if (alpha < 0)
				alpha = this->alpha;
			int sm = radius + MAX(size1, size2) + 4;
			cv::Rect r(c.x - sm, c.y - sm, 2 * sm, 2 * sm);
			cv::Mat mr = m(r);
			cv::Mat s = mr.clone();
			if (shape == Shape::Circle)
			{
				if (size2 > 0) cv::circle(m, c, radius, color2, size2);
				cv::circle(m, c, radius, color1, size1);
			}
			if (shape == Shape::Box)
			{
				if (size2 > 0) cv::rectangle(m, cv::Rect(c.x - radius, c.y - radius, 2 * radius, 2 * radius), color2, size2);
				cv::rectangle(m, cv::Rect(c.x - radius, c.y - radius, 2 * radius, 2 * radius), color1, size1);
			}

			if (shape == Shape::Cross)
			{
				if (size2 > 0) cv::drawMarker(m, c, color2, cv::MARKER_CROSS, 2 * radius, size2);
				cv::drawMarker(m, c, color1, cv::MARKER_CROSS, 2 * radius, size1);
			}

			cv::addWeighted(mr, alpha, s, 1 - alpha, 0, mr);
		}
		catch (...)
		{
			std::cout << "Oops..";
		}
	}

	void Draw(cv::Mat& m, cv::Point2f c, cv::Size cScale, float alpha = -1)
	{
		Draw(m, cv::Point2f(c.x * cScale.width, c.y * cScale.height), alpha);
	}
};

class ByteMatAt
{
	// Layout of RGB/BGR Mats:
	// [0,0] r g b [r=1] r g b [r=2] .. [c=1, r=0] r g b [r=1] r g b .. ..

	cv::Mat m;
public:
	size_t rowstep = 1, colstep = 1;

	ByteMatAt(cv::Mat mat)
	{
		m = mat;
		rowstep = m.step1(0);
		if (m.channels() > 1)
			colstep = m.step1(1);
	}

	uchar At(int x, int y, int c = 0)
	{
		return m.data[c + x * colstep + y * rowstep];
	}

	void Set(uchar v, int x, int y, int c = 0)
	{
		m.data[c + x * colstep + y * rowstep] = v;
	}

	uint64_t GetRGB16At(int x, int y)
	{
		uchar* p = &m.data[x * colstep + y * rowstep];
		return ((uint64_t)p[0] << 32) | ((uint64_t)p[1] << 16) | p[2];
	}
	void SetRGB16At(uint64_t v, int x, int y)
	{
		uchar* p = &m.data[x * colstep + y * rowstep];
		p[0] = (uchar)(v >> 32);
		p[1] = (uchar)(v >> 16);
		p[2] = (uchar)(v);
	}
};

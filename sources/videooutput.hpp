//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <opencv2/opencv.hpp>
#include "config.hpp"
#include "util.hpp"

class VideoOutput
{
public:
	cv::VideoWriter vw;

	void Start(Config& c, std::string path, double fps, cv::Size size)
	{
		//int fourcc = cv::VideoWriter::fourcc('h','v','c','1');
		//int fourcc = cv::VideoWriter::fourcc('X','V','I','D');
		//int fourcc = cv::VideoWriter::fourcc( M','P','4','2');
		//int fourcc = cv::VideoWriter::fourcc('X','2','6','4');
		//int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');

		std::string fcc = c.GetString("OutFOURCC", "mp4v");
		int fourcc = cv::VideoWriter::fourcc(fcc[0], fcc[1], fcc[2], fcc[3]);
		std::string outExt = c.GetString("OutExt", ".mp4");
		int outQuality = c.GetInt("OutQuality", -1);
		std::filesystem::path p(path);
		if (!p.has_extension() || p.extension().string() != outExt)
		{
			p.replace_extension(outExt);
			path = p.string();
		}

		vw.open(path, fourcc, fps, size);
		if (outQuality != -1)
			vw.set(cv::VIDEOWRITER_PROP_QUALITY, outQuality);
		//auto q = vw.get(cv::VIDEOWRITER_PROP_QUALITY);
		//std::cout << "VW Q " << q << std::endl;
	}

	void Write(cv::Mat& frame)
	{
		if (vw.isOpened())
			vw.write(frame);
	}

	void Close()
	{
		vw.release();
	}
};


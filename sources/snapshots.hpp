//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <vector>
#include <sstream>
#include <ostream>
#include <opencv2/opencv.hpp>

#include "config.hpp"

class SnapShots
{
public: 
	float secsPerSnapshot = 60;
	float secsDone = 0;
	std::vector<cv::Mat> snapshots;
	std::string snapshotsPath;

	int saveSnapshots = 0;
	int thumbnailsImageWidth = 640;
	int thumbnailsSheetWidth = 1920;

	SnapShots(Config& c)
	{
		secsPerSnapshot = c.GetFloat("SecsPerSnapshot", 0);
		saveSnapshots = c.GetInt("SaveSnapshots", 0);
		thumbnailsImageWidth = c.GetInt("ThumbnailsImageWidth", 0);
		thumbnailsSheetWidth = c.GetInt("ThumbnailsSheetWidth", 0);
	}

	void Frame(cv::Mat& img, float secs)
	{
		if (thumbnailsImageWidth <= 0 && thumbnailsSheetWidth <= 0)
			return;

		secsDone += secs;
		if (secsPerSnapshot > 0 && secsDone >= secsPerSnapshot)
		{
			secsDone -= secsPerSnapshot;
			cv::Mat im = img.clone();
			snapshots.push_back(im);
			if (snapshotsPath.size() > 0 && saveSnapshots > 0)
			{
				std::ostringstream os;
				os << snapshotsPath << "_" << std::setfill('0') << std::setw(4) << snapshots.size() << ".png";
				std::string path(os.str());
				cv::imwrite(path, im);
				saveSnapshots--;
			}
		}
	}

	void CreateThumbnails()
	{
		auto num = snapshots.size();
		if (thumbnailsImageWidth < 8 || thumbnailsSheetWidth < 8 || num == 0) return;

		std::ostringstream os;
		os << snapshotsPath << "_" << "thumbs.png";
		std::string path(os.str());


		int nx = int(thumbnailsSheetWidth / thumbnailsImageWidth);
		int ny = int((snapshots.size() + nx - 1) / nx);

		int ow = snapshots[0].cols;
		int oh = snapshots[0].rows;
		int tw = thumbnailsImageWidth;
		float scale = (float)tw / ow;
		int th = (int)(oh * scale + 0.5f);

		cv::Mat thumb;
		cv::Mat sheet(cv::Size(int(nx * tw), int(ny * th)), CV_8UC3);
		for (int i = 0; i < num; i++)
		{
			int x = i % nx;
			int y = i / nx;
			cv::resize(snapshots[i], thumb, cv::Size(tw, th), 0, 0, cv::INTER_AREA);
			thumb.copyTo(sheet(cv::Rect(x * tw, y * th, tw, th)));
		}

		cv::imwrite(path, sheet);
	}
};


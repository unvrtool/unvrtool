//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <opencv2/opencv.hpp>
#include "videoframe.hpp"
#include "util.hpp"

class VideoInput
{
private:
	int setNextFrame = -1;

public:
	int frameSpeed = 1;
	double fps = 0;
	int frameCount = 0;
	bool pause = false;

	bool capRun = true;
	bool capRunning = false;
	cv::VideoCapture cap;
	BlockingQueue<VideoFrame*> frame_capt;
	BlockingQueue<VideoFrame*> frame_free;
	std::thread* captThread = nullptr;

	VideoInput()
	{
		frame_free.push(new VideoFrame());
		frame_free.push(new VideoFrame());
	}

	float SecsPerImage()
	{
		return (float)(frameSpeed / fps);
	}

	void SetNextFrame(int frame) 
	{
		if (frame < 0) frame = 0;
		if (frame >= frameCount) frame = frameCount - 1;
		setNextFrame = frame; 
	}

	bool IsRunning() 
	{
		return capRunning;
	}

	void Close()
	{
		if (captThread != nullptr)
		{
			if (capRunning)
			{
				capRun = false;
				for(int i=0; i<50 && capRunning; i++)
					std::this_thread::sleep_for(std::chrono::milliseconds(10));

				captThread->detach();
			}
		}
		if (cap.isOpened())
			cap.release();
	}

	bool Open(const std::string& videopath)
	{
		cap.setExceptionMode(true);
		try
		{
			cap.open(videopath);
		}
		catch (...)
		{
			std::cerr << what();
		}
		if (!cap.isOpened()) {
			std::cout << "Error opening video stream or file" << std::endl;
			return false;
		}

		fps = cap.get(cv::CAP_PROP_FPS);
		frameCount = (int)cap.get(cv::CAP_PROP_FRAME_COUNT);
		return true;
	}

	void Start()
	{
		captThread = new std::thread([&]() {
			try
			{
				cap.set(cv::CAP_PROP_POS_FRAMES, 0);
				cap.grab();

				while (capRun)
				{
					capRunning = true;
					auto vf = frame_free.wait_pop();

					try
					{
						if (setNextFrame != -1)
						{
							if (setNextFrame >= frameCount)
								setNextFrame = frameCount - 1;
							int frame = setNextFrame;
							setNextFrame = -1;
							cap.set(cv::CAP_PROP_POS_FRAMES, frame);
							cap.grab();
						}
						else
						{
							for (int skip = 0; !pause && skip < frameSpeed; skip++)
								cap.grab(); // advance frame
						}

						int frameNo = (int)cap.get(cv::CAP_PROP_POS_FRAMES) - 1; // get returns *next* frame number..
						vf->SetTimeCode(fps, frameNo);
						cap.retrieve(vf->Frame);
					}
					catch (...) 
					{
						vf->SetTimeCode(fps, -1);
					}
					frame_capt.push(vf);
				}
			}
			catch (...) {}
			capRunning = false;
			});
	}

	VideoFrame* GetFrame()
	{
		if (!capRunning)
		{
			if (frame_capt.empty())
				return nullptr;
		}
		return frame_capt.wait_pop();
	}

	void ReleaseFrame(VideoFrame* frame)
	{
		frame_free.push(frame);
	}
};

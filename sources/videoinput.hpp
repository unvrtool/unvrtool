//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <opencv2/opencv.hpp>
#include "videoframe.hpp"
#include "blockingqueue.hpp"
#include "util.hpp"

class VideoInput
{
private:
	int setNextFrame = -1;
	cv::VideoCapture cap;

	bool capRun = true;
	bool capRunning = false;

	BlockingQueue<VideoFrame*> frame_capt;
	BlockingQueue<VideoFrame*> frame_free;
	std::thread* captThread = nullptr;
	std::string path;

public:
	int frameSpeed = 1;
	double fps = 0;
	int width=0, height=0;
	int frameCount = 0;
	bool pause = false;
	int curframeNo;

	VideoInput()
	{
		frame_free.push(new VideoFrame());
		frame_free.push(new VideoFrame());
	}

	float SecsPerImage()
	{
		return (float)(frameSpeed / fps);
	}

	void SetEndFrame(int frame)
	{
		if (frame < frameCount)
			frameCount = frame;
	}

	void SetNextFrame(int frame) 
	{
		if (frame < 0) frame = 0;
		if (frame >= frameCount) frame = frameCount - 2;
		setNextFrame = frame; 
	}

	bool IsRunning() 
	{
		return capRunning;
	}

	bool _DoOpen()
	{
		cap.setExceptionMode(true);
		try
		{
			cap.open(path);
		}
		catch (...)
		{
			std::cerr << what();
		}
		return cap.isOpened();
	}

	void _DoClose()
	{
		try
		{
			if (cap.isOpened())
				cap.release();
		}
		catch(...) 
		{
			std::cerr << "_DoClose: " << what();
		}
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
		_DoClose();
	}

	bool Open(const std::string& videopath)
	{
		path = std::string(videopath);

		captThread = new std::thread([&]() {
			try
			{
				setNextFrame = -1;
				auto vf = frame_free.wait_pop();

				if (!_DoOpen()) {
					std::cout << "Error opening video stream or file" << std::endl;
					capRun = false;
					vf->SetTimeCode(fps, -1);
					frame_capt.push(vf);
				}
				else
				{
					capRunning = true;
					fps = cap.get(cv::CAP_PROP_FPS);
					width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
					height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);

					frameCount = (int)cap.get(cv::CAP_PROP_FRAME_COUNT);
					cap.grab();

					int frameNo = (int)cap.get(cv::CAP_PROP_POS_FRAMES) - 1; // get returns *next* frame number..
					vf->SetTimeCode(fps, frameNo);
					cap.retrieve(vf->Frame);
					frame_capt.push(vf);

					// Add it again since the first is consumed by status check
					vf = frame_free.wait_pop();
					vf->SetTimeCode(fps, frameNo);
					cap.retrieve(vf->Frame);
					frame_capt.push(vf);

					while (capRun)
					{
						auto vf = frame_free.wait_pop();

						try
						{
							if (setNextFrame != -1)
							{
								// Flush all captured frames
								while (frame_capt.try_pop(vf))
									frame_free.push(vf);

								if (setNextFrame >= frameCount)
									setNextFrame = frameCount - 2;
								curframeNo = setNextFrame;
								setNextFrame = -1;
								cap.set(cv::CAP_PROP_POS_FRAMES, curframeNo);
								int errCnt = 0;
								bool ok = false;
								while (!ok && errCnt < 5)
								{
									try
									{
										cap.grab();
										ok = true;
									}
									catch (...)
									{
										errCnt++;
										if (curframeNo > 0 && curframeNo > frameCount - 1000)
										{
											int q = 2 * (errCnt * errCnt);
											curframeNo -= q;
											cap.set(cv::CAP_PROP_POS_FRAMES, curframeNo);
										}
									}
								}
							}
							else
							{
								for (int skip = 0; !pause && skip < frameSpeed; skip++)
									cap.grab(); // advance frame
							}

							curframeNo = (int)cap.get(cv::CAP_PROP_POS_FRAMES) - 1; // get returns *next* frame number..
							if (curframeNo >= frameCount)
								vf->SetTimeCode(fps, -1);
							else
							{
								cap.retrieve(vf->Frame);
								if (setNextFrame != -1)
								{
									frame_free.push(vf);
									continue;
								}
								vf->SetTimeCode(fps, curframeNo);
							}
						}
						catch (...)
						{
							std::cout << "Video Error at frame " << curframeNo << std::endl;

							std::cout << what() << std::endl;
							vf->SetTimeCode(fps, -1);
						}
						frame_capt.push(vf);
					}
				}
			}
			catch (...) 
			{
				std::cout << "VideoIn thread exception: " << what() << std::endl;
			}
			capRunning = false;
			std::cout << "VideoIn thread exit" << std::endl;
			});

		VideoFrame* f = frame_capt.wait_pop();
		bool ok = f->FrameNo != -1;
		ReleaseFrame(f);
		return ok;
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

	void SkipFrame()
	{
		VideoFrame* f = GetFrame();
		ReleaseFrame(f);
	}

};


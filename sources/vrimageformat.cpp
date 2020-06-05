//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <opencv2/features2d.hpp>

#include "util.hpp"
#include "vrimageformat.hpp"

bool 
VrImageFormat::CheckStereo(cv::Mat img1, cv::Mat img2, bool horizontal)
{
	const int MedianYMatchErrorThreshold = 3;
	const int MinGoodMatchesThreshold = 30;

	const float nn_match_ratio = 0.8f;   // Nearest neighbor matching ratio

	std::vector<cv::KeyPoint> kpts1, kpts2;
	cv::Mat desc1, desc2;
	cv::Ptr<cv::AKAZE> akaze = cv::AKAZE::create();
	akaze->detectAndCompute(img1, cv::noArray(), kpts1, desc1);
	if (kpts1.size() < MinGoodMatchesThreshold * 3)
	{
		akaze.release();
		akaze = cv::AKAZE::create(cv::AKAZE::DESCRIPTOR_MLDB, 0, 3, 0.0001f);
		akaze->detectAndCompute(img1, cv::noArray(), kpts1, desc1);
		if (kpts1.size() < MinGoodMatchesThreshold * 3)
		{
			akaze.release();
			akaze = cv::AKAZE::create(cv::AKAZE::DESCRIPTOR_MLDB, 0, 3, 0.00001f);
			akaze->detectAndCompute(img1, cv::noArray(), kpts1, desc1);
		}
	}

	akaze->detectAndCompute(img2, cv::noArray(), kpts2, desc2);

	cv::BFMatcher matcher(cv::NORM_HAMMING);
	std::vector<std::vector<cv::DMatch> > nn_matches;
	matcher.knnMatch(desc1, desc2, nn_matches, 2);

	std::vector<float> dx, dy;
	std::vector<cv::KeyPoint> matched1, matched2;
	for (size_t i = 0; i < nn_matches.size(); i++) {
		cv::DMatch first = nn_matches[i][0];
		float dist1 = nn_matches[i][0].distance;
		float dist2 = nn_matches[i][1].distance;
		if (dist1 < nn_match_ratio * dist2) {
			auto p1 = kpts1[first.queryIdx];
			auto p2 = kpts2[first.trainIdx];
			dx.push_back(abs(p2.pt.x - p1.pt.x));
			dy.push_back(abs(p2.pt.y - p1.pt.y));
			matched1.push_back(kpts1[first.queryIdx]);
			matched2.push_back(kpts2[first.trainIdx]);
		}
	}
	if (dx.size() == 0)
		return false;

	float matchRatio = dx.size() / (float)kpts1.size();
	float medx = util::median(dx);
	float medy = util::median(dy);
	std::cout << "Med X " << medx << "Med Y " << medy << " Match ratio " << matchRatio << "  Pts " << kpts1.size() << std::endl;

	if (dx.size() < MinGoodMatchesThreshold)
		return false;
	if (medy > MedianYMatchErrorThreshold)
		return false;

	//std::vector<cv::DMatch> good_matches;
	//for (size_t i = 0; i < matched1.size(); i++)
	//	good_matches.push_back(cv::DMatch(i, i, 0));

	//cv::Mat res;
	//cv::drawMatches(img1, matched1, img2, matched2, good_matches, res);
	//imshow("result", res);
	//cv::waitKey();
	return true;
}

void
VrImageFormat::Detect(int confirmations, cv::VideoCapture cap)
{
	//cv::VideoCapture& cap = vidIn->cap;
	auto fps = cap.get(cv::CAP_PROP_FPS);
	auto fc = cap.get(cv::CAP_PROP_FRAME_COUNT);

	Detect(confirmations, [&](int frameNo, int frameTot)
		{
			auto cf = (int)(fc * frameNo / frameTot);
			auto ms = 1000.0 * cf / fps;
			cap.set(cv::CAP_PROP_POS_MSEC, ms);
			for (int s = 0; s < 10; s++)
				cap.grab();
			cv::Mat mc;
			cap.retrieve(mc);
			return mc;
		});

	cap.set(cv::CAP_PROP_POS_MSEC, 0); // Rewind
}

float MedianDevFromEllipse(cv::RotatedRect& box, std::vector<cv::Point>& contour)
{
	std::vector<float> errs;
	errs.reserve(contour.size());

	auto c = box.center;
	float irw = 2.0f / box.size.width;
	float irh = 2.0f / box.size.height;
	for (cv::Point p : contour)
	{
		float x = (p.x - c.x) * irw;
		float y = (p.y - c.y) * irh;
		float r2 = x * x + y * y;
		if (r2 > 1) r2 = 1 / r2; // Keep in range 0..1
		errs.push_back(r2);
	}
	float merr = util::median(errs);
	merr = 1 - sqrt(merr);
	return merr;
}

std::vector<cv::Rect> CheckSpherical(cv::Mat imageOrg)
{
	cv::Mat rgbIm;
	cv::cvtColor(imageOrg, rgbIm, cv::COLOR_GRAY2BGR);
	const int pad = 100;

	std::vector<cv::Rect> l;
	int w = imageOrg.cols;
	int h = imageOrg.rows;
	cv::Mat im(h+2*pad, h+2*pad, CV_8UC1);
	im = 0;
	imageOrg.copyTo(im(cv::Rect(pad, pad, w, h)));

	// Dillate or something might improve results on jagged edges..
	//cv::medianBlur(im, im, 5);

	im *= 50;
	//cv::imshow("Spherical", im);
	//cv::waitKey();

	size_t maxR = MIN(imageOrg.cols, imageOrg.rows)/2;
	size_t minR = 9 * maxR / 10;
	size_t minCircleArea = 3.14f * minR * minR;
	size_t expCircleArea = 3.14f * maxR * maxR;

	//cv::dilate(im, im, );
	//cvFindContours(src, storage, &contour, sizeof(CvContour), CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(im, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

	for (size_t i = 0; i < contours.size(); i++)
	{
		auto contour = contours[i];
		if (contour.size() < 100)
			continue;

		double actual_area = cv::contourArea(contour);
		if (actual_area < minCircleArea)
			continue;

		float prc = 100 * actual_area / expCircleArea;

		cv::Mat pointsf;
		cv::Mat(contour).convertTo(pointsf, CV_32F);

		// In case of clipped ellipse the found size will be a litte too small, but should be reasonably ok
		cv::RotatedRect box = fitEllipse(pointsf);
		float medErr = MedianDevFromEllipse(box, contour);
		if (medErr > 0.03) // >50% points have more than 3% deviation from found ellipse?
			continue;

		auto c = box.center - cv::Point2f(pad, pad);
		cv::RotatedRect box2(c, box.size, 0);

		float w = box2.size.width;
		float  h = box2.size.height;
		float x0 = c.x - w / 2;
		float x1 = c.x + w / 2;
		float y0 = c.y - h / 2;
		float y1 = c.y + h / 2;

		int ix0 = round(x0);
		int ix1 = round(x1);
		int iy0 = round(y0);
		int iy1 = round(y1);

		std::cout << "X " << x0 << " - " << x1 << " = " << (ix1 - ix0) << std::endl;
		std::cout << "Y " << y0 << " - " << y1 << " = " << (iy1 - iy0) << std::endl;

		cv::Rect rr(ix0, iy0, ix1 - ix0, iy1 - iy0);
		l.push_back(rr);
		cv::RotatedRect box3(rr.tl(), rr.tl() + cv::Point2i(rr.width, 0), rr.br());

		cv::ellipse(rgbIm, box3, cv::Scalar(0, 0, 255), 1, 8);
		//cv::imshow("Contour", rgbIm);
		//cv::waitKey();
	}
	return l;
}

void 
VrImageFormat::Detect(int confirmations, std::function<cv::Mat(int, int)> getFrame)
{
	cv::Mat mc, mf, m;
	int cLR = 0, cTB = 0, cM = 0, cU = 0, cT = 0;
	std::vector<std::pair<cv::Rect, cv::Rect>> sphereList;

	for (int p = 1; p < 10; p += 1)
	{
		mc = getFrame(p, 10);

		cv::cvtColor(mc, mf, cv::COLOR_BGR2GRAY);
		int smin = MIN(mf.cols, mf.rows);
		float downscale = 800.0f / smin;
		cv::resize(mf, m, cv::Size(), downscale, downscale, cv::INTER_AREA);

		auto sl1 = CheckSpherical(mf(cv::Rect(0, 0, mf.cols / 2, mf.rows)));
		if (sl1.size() == 1)
		{
			auto sl2 = CheckSpherical(mf(cv::Rect(mf.cols / 2, 0, mf.cols / 2, mf.rows)));
			if (sl2.size() == 1)
				sphereList.emplace_back(sl1[0], sl2[0]);
		}

		int w = m.cols / 8;
		int h = m.rows / 8;
		cv::Mat x0 = m(cv::Rect(w * 1, h * 2, w * 2, h * 4)).clone();
		cv::Mat x1 = m(cv::Rect(w * 5, h * 2, w * 2, h * 4)).clone();
		cv::Mat y0 = m(cv::Rect(w * 2, h * 1, w * 4, h * 2)).clone();
		cv::Mat y1 = m(cv::Rect(w * 2, h * 5, w * 4, h * 2)).clone();
		bool lr = CheckStereo(x0, x1, true);
		bool tb = CheckStereo(y0, y1, false);
		cT++;
		if (!lr && !tb) cM++;
		if (lr && !tb) cLR++;
		if (!lr && tb) cTB++;
		if (lr && tb) cU++;

		if (cT > confirmations)
		{
			if (cM > confirmations && cM > cT * 3 / 4) SetMono();
			if (cLR > confirmations && cLR > cT * 3 / 4) SetLeftRight();
			if (cTB > confirmations && cTB > cT * 3 / 4) SetTopBottom();
			if (IsLayoutSet())
				break;
		}
	}

	if (IsLayoutSet())
	{
		subImageRects.push_back(GetSubImg(mf.size(), 0));
		subImageRects.push_back(GetSubImg(mf.size(), 1));
		cv::Size s = GetSubImgSize(mf.size());

		if (sphereList.size() > 0)
		{
			Set180(VrImageGeometryMapping::Type::Spherical);
			sphericalEllipseRects.push_back(SphericalTextureAdjust(s, sphereList[0].first));
			sphericalEllipseRects.push_back(SphericalTextureAdjust(s, sphereList[0].second));
		}
		else
		{
			if (s.width == s.height)
				Set180();
			else if (s.width == s.height * 2)
				Set360();
			else if ((s.width == 1920 || s.width == 1920 / 2) && (s.height == 1080 || s.height == 1080 / 2))
				SetStereoscopic();
			else
				std::cerr << "Strange aspect ratio " << s.width << ":" << s.height << std::endl;
		}
	}
	else
		std::cerr << "Unable to determine layout" << std::endl;
}

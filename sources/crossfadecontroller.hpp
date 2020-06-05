//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "vectorwindow.hpp"
#include "domainspacetransform.hpp"
#include "controllerbase.hpp"

class CrossFadeController : public ControllerBase
{
	DomainSpaceTransform dst;
	VectorWindow<float> ww;

	void Add(float v)
	{
		float t = dst.TransformForward(v);
		ww.Add(t);
	}

public:

	CrossFadeController() { ww.SetWindowType(WindowType::Sinusoidal); }

	CrossFadeController(float initVal, float fps = 0, float secs = 0) 
	{
		ww.SetWindowType(WindowType::Sinusoidal);
		if (fps > 0)
			Reset(fps, secs, initVal); 
		else
			Set(initVal, true);
	}

	void SetTransform(DomainSpaceTransform transform)
	{
		dst = transform;
	}

	float Get()
	{
		float t = ww.GetWeightedAverage();
		float v = dst.TransformInverse(t);
		return v;
	}

	void Reset(float fps, float param, float val)
	{
		SetFps(fps);
		int N = int(0.5f + param * fps);
		if (N < 1) N = 1;
		ww.Reset(N);
		Set(val, true);
	}

	void Set(float val, bool instantly)
	{
		if (util::isBad(val))
			return;
		val = LimitVal(val);
		_targetVal = val;

		if (instantly)
		{
			_curVal = val;
			float t = dst.TransformForward(val);
			ww.Set(t);
		}
	}
	
	float Update(float dSecs)
	{
		int frames = (int)roundf(dSecs * _fps);
		for (int i = 0; i < frames; i++)
			Add(_targetVal);

		_curVal = Get();
		return _curVal;
	}
};

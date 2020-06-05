//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <limits>

class ControllerBase
{
protected:
	float _curVal = 0, _targetVal = 0;
	float _valMin, _valMax;
	float _fps = 0;

public:

	float curVal() { return _curVal; }

	ControllerBase()
	{
		_valMin = -std::numeric_limits<float>::max();
		_valMax = std::numeric_limits<float>::max();
	}

	float LimitVal(float val)
	{
		if (val > _valMax) return _valMax;
		if (val < _valMin) return _valMin;
		return val;
	}

	virtual void Reset(float fps, float param, float val) = 0;
	virtual void Set(float val, bool instantly) = 0;
	virtual float Update(float dSecs) = 0;

	void Change(float dVal, bool instantly)
	{
		Set((instantly ? _curVal : _targetVal) + dVal, instantly);
	}

	void SetTarget(float val) { Set(val, false); }
	void SetCurrent(float val) { Set(val, true); }

	void SetLimits(float min, float max)
	{
		_valMin = min;
		_valMax = max;
	}

	void SetLimits(float max) { SetLimits(-max, max); }

	void SetFps(float fps)
	{
		_fps = fps;
	}
};
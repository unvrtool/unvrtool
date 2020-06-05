//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <functional>

class DomainSpaceTransform
{
public:
	typedef std::function<float(float)> FloatFunc;

	FloatFunc TransformForward;
	FloatFunc TransformInverse;

	void Set(FloatFunc forwardTransform, FloatFunc inverseTransform)
	{
		TransformForward = forwardTransform;
		TransformInverse = inverseTransform;
	}

	DomainSpaceTransform()
	{
		FloatFunc id = [](float v) { return v; };
		Set(id, id);
	}

	DomainSpaceTransform(FloatFunc forwardTransform, FloatFunc inverseTransform)
	{
		Set(forwardTransform, inverseTransform);
	}

	DomainSpaceTransform static Inverse()
	{
		FloatFunc inv = [](float v) { return 1 / v; };
		return DomainSpaceTransform(inv, inv);
	}
};
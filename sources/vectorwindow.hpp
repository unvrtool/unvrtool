//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <vector>

#include "util.hpp"

enum class WindowType { Rectangular = 0, Triangular = 1, Sinusoidal = 2 };

template <class T>
class VectorWindow
{
public:

private:
	int cur = 0;
	int N = 0;
	WindowType wtype = WindowType::Rectangular;
	std::vector<T> values;
	std::vector<float> weights;
	float weightsTotal = 1;

	T& At(int i) 
	{
		return values.at((i + cur) % N);
	}

	float GenerateWeight(float i)
	{
		switch (wtype)
		{
		case WindowType::Rectangular: return 1;
		case WindowType::Triangular: return  i < 0.5 ? 2 * i : 2 * (1 - i);
		case WindowType::Sinusoidal: return 1 - cosf(2*util::pi * i);

		default: throw std::exception("Unknow wtype enum");
		}
	}

	void GenerateWeights()
	{
		weights.reserve(N);
			for (int i = 0; i < N; i++)
			{
				float fi = (i+1) / float(N + 1); // 0 & 1 usually gives 0 so no need for those
				float v = GenerateWeight(fi);
				weights.push_back(v);
			}

		double tot = 0;
		for (int i = 0; i < N; i++)
			tot += weights[i];
		weightsTotal = (float)tot;
	}

public:

	void SetWindowType(WindowType type)
	{
		wtype = type;
	}


	void Reset(int size) { cur = 0; N = size; values.reserve(N); GenerateWeights(); }

	void Set(const T& val) { values.clear(); for (int i = 0; i < N; i++) values.push_back(val); }


	VectorWindow(int size = 0) { Reset(size); }

	void Add(T e)
	{
		if (values.size() < N)
			values.push_back(e);
		else
			values.at(cur++) = e;
		if (cur == N)
			cur = 0;
	}

	T GetAverage()
	{
		T a = values.at(0);
		for (int i=1; i<values.size(); i++)
			a = a + values.at(i);
		a = a * (1.0f / values.size());
		return a;
	}

	T GetWeightedAverage()
	{
		T a = 0;
		for (int i = 0; i < values.size(); i++)
			a = a + weights[i] * At(i);
		a = a * (1.0f / weightsTotal);
		return a;
	}
};
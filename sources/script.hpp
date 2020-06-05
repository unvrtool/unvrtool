//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <string>
#include <map>

#include "camparams.hpp"

class ScriptItem : public Csp
{
public:
	int Frame = 0;

	ScriptItem() {}
	ScriptItem(int frame, Csp csp) : Csp(csp), Frame(frame) {}

	ScriptItem(std::ifstream& s, int frame)
	{
		Frame = frame;
		std::string t;
		for (Cp::Enum p : { Cp::Yaw, Cp::Pitch, Cp::Fov, Cp::Bo })
		{
			s >> t;
			if (t != "-")
			{
				float v = std::stof(t);
				Set(p, v);
			}
		}
	}


	void Write(std::ofstream& s)
	{
		//s << Frame << " ";
		for (Cp::Enum p : { Cp::Yaw, Cp::Pitch, Cp::Fov, Cp::Bo })
			if (IsSet(p))
				s << (*this)[p] << "\t";
			else
				s << "-\t";
		s << "\n";
	}

};

class Script
{
public:
	std::map<int, ScriptItem> items;

	ScriptItem* Get(int frame)
	{
		auto i = items.find(frame);
		return i == items.end() ? nullptr : &i->second;
	}

	ScriptItem* GetBefore(int frame)
	{
		auto elt = items.lower_bound(frame);
		if (elt == items.begin()) return nullptr;
		return &(--elt)->second;
	}

	ScriptItem* GetAfter(int frame)
	{
		auto elt = items.upper_bound(frame);
		if (elt == items.end()) return nullptr;
		return &elt->second;
	}

	void Delete(int frameNo)
	{
		if (Get(frameNo))
			items.erase(frameNo);
	}

	void Set(ScriptItem si) 
	{
		items[si.Frame] = si;
	}

	void Set(int frame, Csp csp) 
	{ 
		Set(ScriptItem(frame, csp)); 
	}

	void Save(std::ofstream& s, float fps)
	{
		s << "Unvrtool script v1.0\n";
		for (auto& kv : items) {
			auto& i = kv.second;
			TimeCodeHMS t(i.Frame/ fps);
			s << t.ToString() << "\t";
			kv.second.Write(s);
		}
	}

	void Load(std::ifstream& s, float fps)
	{
		items.clear();

		std::string t;
		std::getline(s, t);
		if (t != "Unvrtool script v1.0")
			throw std::exception("Invalid scriptfile");

		while (!s.eof())
		{
			t.clear();
			s >> t;
			if (t.size() < 11 || t.size() > 12)
				break;
			TimeCodeHMS tc = TimeCodeHMS(t);
			int frame = (int)roundf(tc.ToSecs() * fps);
			ScriptItem si(s, frame);
			Set(si);
		}
	}

	static std::string DefExt() { return std::string(".uvrtscript"); }


	void Save(std::string path, float fps)
	{
		std::filesystem::path p(path);
		if (!p.has_extension()) 
			p = p.replace_extension(DefExt());

		std::ofstream cFile(p);
		if (cFile.is_open())
			Save(cFile, fps);
	}

	void Load(std::string path, float fps)
	{
		std::filesystem::path p(path);
		if (!p.has_extension())
			p = p.replace_extension(DefExt());

		std::ifstream cFile(p);
		if (cFile.is_open())
			Load(cFile, fps);
	}

};
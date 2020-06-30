//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <istream>
#include <algorithm>
#include <vector>
#include <functional>
#include <limits>
#include <filesystem>
#include <assert.h>

class SimpleConfig
{
private:
	std::map<std::string, std::string> kv;
	std::vector<std::string> lines;

	int stoi(std::string s)
	{
		if (s == "All" || s == "all") return std::numeric_limits<int>::max();
		return std::stoi(s);
	}

	std::string lc(std::string s, bool copy = true)
	{
		if (copy)
			s = std::string(s);
		for (auto& c : s)
			if (c >= 'A' && c <= 'Z')
				c = c - ('A' - 'a');
		return s;
	}

	std::string* GetP(const std::string& key)
	{
		auto f = kv.find(lc(key));
		return  f == kv.end() ? nullptr : &f->second;
	}

protected:
	bool keepBlankLines = true;

public:
	SimpleConfig() { }

	SimpleConfig(const SimpleConfig& c)
	{
		kv = std::map<std::string, std::string>(c.kv);
		lines = std::vector<std::string>(c.lines);
	}

	bool Get(const std::string& key, std::string& val)
	{
		std::string* pVal = GetP(key);
		val.clear();
		if (pVal) val = *pVal;
		return pVal;
	}

	std::string Get(const std::string& key)
	{
		std::string* s = GetP(key);
		assert(s);
		return std::string(*s);
	}


	bool Has(const std::string& key) { return GetP(key) != nullptr; }

	const std::string  GetString(const std::string& key, const char* defVal) { auto v = GetP(key); return v ? *v : std::string(defVal); }

	int GetInt(const std::string& key) { return stoi(Get(key)); }
	int GetInt(const std::string& key, int defVal) { auto v = GetP(key); return v ? stoi(*v) : defVal; }

	float GetFloat(const std::string& key) { return stof(Get(key)); }
	float GetFloat(const std::string& key, float defVal) { auto v = GetP(key); return v ? stof(*v) : defVal; }


	void Set(const std::string& key, std::string value)
	{
		assert(key == "" || key[0] != ' ');
		std::string lKey = lc(key);
		auto f = kv.find(lKey);
		bool newKey = f == kv.end();

		if (!keepBlankLines || key != "")
		{
			if (key != "" && key[0] == ' ')
				return; // Don't keep blank lines

			if (!newKey)
			{
				f->second = value;
				return; // Not a new key, just changing an existing key
			}
		}

		// New key/line
		lines.push_back(key);
		if (key != "" && key[0] != '#')
			kv.emplace(lKey, value);
	}

	template <class StreamType>
	void Read(StreamType& stream)
	{
		std::string lineOrg;
		while (getline(stream, lineOrg))
		{
			std::string line(lineOrg);

			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			if (line.empty()) Set("", "");
			else if (line[0] == '#') Set(lineOrg, ""); // Keep comments as they were
			else
			{
				auto delimiterPos = line.find("=");
				if (delimiterPos == std::string::npos)
					std::cerr << "!! Warning: No = found in line: " << lineOrg << std::endl;
				else
				{
					// TODO: Should have special handling for quoted strings with whitespace
					auto key = line.substr(0, delimiterPos);
					auto value = line.substr(delimiterPos + 1);
					Set(key, value);
				}
			}
		}
	}

	void ReadString(std::string input)
	{
		std::string line;
		std::stringstream ss(input);

		while (getline(ss, line, ';'))
		{
			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			auto delimiterPos = line.find("=");
			auto key = line.substr(0, delimiterPos);
			auto value = line.substr(delimiterPos + 1);
			Set(key, value);
		}
	}

	bool ReadFile(const char* path, const char* optPath = nullptr)
	{
		std::filesystem::path p(path);
		if (!p.has_extension())
			p = p.replace_extension(".config");
		if (optPath && !std::filesystem::exists(p))
			p = std::filesystem::path(optPath) / p;

		std::ifstream cFile(p);
		if (!cFile.is_open())
			return false;

		Read(cFile);
		return true;
	}


	void Write(const char* path)
	{
		std::ofstream cFile(path);
		if (cFile.is_open())
		{
			for (auto const& key : lines)
				if (key == "" || key[0] == '#')
					cFile << key << std::endl;
				else
					cFile << key << " = " << Get(key) << std::endl;
		}
	}

	std::string Print(int level)
	{
		std::ostringstream os;

		for (auto const& key : lines)
		{
			if (key == "") { if (level > 2) os << std::endl; }
			else if (key[0] == '#') { if (level > 2) os << key << std::endl; }
			else if (level == 1) os << key << " = " << Get(key) << std::endl;
			else os << key << "=" << Get(key) << "; ";
		}
		return os.str();
	}
};

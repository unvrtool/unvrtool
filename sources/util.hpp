//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "_headers_std.hpp"

#if defined(_WIN32) || defined(WIN32)
#define __win32__ 1
#endif

std::string what(const std::exception_ptr& eptr = std::current_exception());

template <typename T>
std::string nested_what(const T& e)
{
	try { std::rethrow_if_nested(e); }
	catch (...) { return " (" + what(std::current_exception()) + ")"; }
	return {};
}

inline std::string what(const std::exception_ptr& eptr)
{
	if (!eptr) { throw std::bad_exception(); }

	try { std::rethrow_exception(eptr); }
	catch (const std::exception & e) { return e.what() + nested_what(e); }
	catch (const std::string & e) { return e; }
	catch (const char* e) { return e; }
	catch (...) { return "Unknown exceptiontype"; }
}


namespace util
{
	const float pi = static_cast<float>(3.14159265358979323846264338327950288);
	constexpr float rad(float deg) noexcept { return deg * pi / 180; }
	constexpr float deg(float rad) noexcept { return rad * 180 / pi; }

	constexpr bool isBad(float v) noexcept { return v != 0 && !isnormal(v); }


	std::string GetAppFolderPath();
	bool RunProcess(std::string app, std::string args);
	void CheckOpenCvDlls();

	template <typename T>
	T median(const std::vector<T>& vorg)
	{
		std::vector<T> v(vorg);
		size_t n = v.size() / 2;
		std::nth_element(v.begin(), v.begin() + n, v.end());
		return v[n];
	}

	bool GetArgsFrom(const char* path, int* argc, char*** argv);
};


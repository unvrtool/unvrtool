//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include "_headers_std_cv.hpp"
#include <assert.h>
#include "util.hpp"

#if __win32__
#include <windows.h>
#endif

using namespace std;

namespace util
{
	string GetAppFolderPath()
	{
#if __win32__
		WCHAR wpath[MAX_PATH];
		GetModuleFileNameW(NULL, wpath, MAX_PATH);
		return filesystem::path(wpath).parent_path().string() + "\\";
#else
#pragma message ( "warning: GetAppFolderPath() not implemented for this architecure" )
			return "";
#endif
	}

#if __win32__
	inline std::wstring ConvertToWide(const std::string& as)
	{
		if (as.empty())    return std::wstring();

		size_t reqLength = ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), 0, 0);

		std::wstring ret(reqLength, L'\0');
		::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), &ret[0], (int)ret.length());
		return ret;
	}
#endif

	bool RunProcess(std::string app, std::string args)
	{
#if __win32__
		auto abspath = std::filesystem::absolute(app);
		if (!std::filesystem::exists(abspath))
		{
			std::string p = GetAppFolderPath() + app;
			auto abspath2 = std::filesystem::absolute(p);
			if (std::filesystem::exists(abspath2))
				abspath = abspath2;
			else
			{
				std::cout << "Unable to find\n" << abspath.string() << "\nor\n" << abspath2.string() << "\n trying plain " << app;
				auto abspath = std::filesystem::path(app);
			}
		}



		auto wapp = abspath.wstring();
		auto wargs = ConvertToWide(args);

		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		// Start the child process. 
		if (!CreateProcess(wapp.c_str(), wargs.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
		{
			cout << "CreateProcess failed: " << GetLastError() << std::endl;
			cout << "Application: " << abspath.string() << std::endl;

			return false;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return true;
#else
#pragma message ( "warning: RunProcess() not implemented for this architecure" )
		return false;
#endif
	}


	// Check OpenCV dlls
	void CheckOpenCvDlls()
	{
		cv::add(cv::Mat(), cv::Mat(), cv::Mat());
	}

	bool GetArgsFrom(const char* path, int* argc, char*** argv)
	{
		std::vector<char*> v;
		v.push_back(**argv); // argv[0]

		std::ifstream ifs(path);
		if (!ifs.is_open()) return false;
		auto* txt = new std::string((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
		char* p = txt->data();
		const char* end = txt->data() + txt->size();

		char* s = p;
		while (*p)
		{
			assert(p < end);
			if (*p == '#')
				for (; *p != '\n' && *p != 0; s = ++p);
			else if (*p <= ' ')
				for (; *p && *p <= ' '; s = ++p);
			else if (*p == '\"')
			{
				for (s = ++p; *p != '\"' && *p != 0; ++p);
				*p = 0;
				v.push_back(s);
				s = ++p;
			}
			else
			{
				for (; *p > ' ' && *p != 0; ++p);
				*p = 0;
				v.push_back(s);
				s = ++p;
			}
		}
		assert(p == end);
		char** av = new char* [v.size()];
		for (int i = 0; i < (int)v.size(); i++)
			av[i] = v[i];

		*argv = av;
		*argc = v.size();
		return true;
	}

};

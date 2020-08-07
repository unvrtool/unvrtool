//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include "_headers_std.hpp"
#include "util.hpp"
#include "config.hpp"
#include "vrrecorder.hpp"

using namespace std;

const char* usage = R"QQ(
Quick Usage: 
unvrtool <video>	             - Show video
unvrtool -save <video>	         - Save video without showing
unvrtool -save -view <video>     - Save video while showing
unvrtool -s -a <video>	         - Save video and then create new video with source-audio
unvrtool -s -t out.mp4 <video>   - Save video to out.mp4
unvrtool -tf out\ path\*.mp4 - Save all mp4 videos in path to out folder 
unvrtool -config <name> -s <video> - Save video using <name> configuration
unvrtool -save <video1> <video2> - Opens <video1> and saves, the opens <video2> and saves
unvrtool -c my -c 640x480 -v -sa -tf out\ path\*.mp4
- Show & save all path\*.mp4 to out\ with audio using "my.config" and "640x480.config"


General Usage: 
unvrtool [options] <path to VR video file1> [<path to VR video file2> [...]]

Options:
<file> or -i <file>          Video to open
-if | -inputformat <format>  Specify video format, ex tb:360:equirectangular, lr:180:fisheye, lr::flat
-v  | -view                  View video
-s  | -save                  Save video(s), without showing anything unless -g specified
-sc | -script				 Allow user to setup camera shots first, esc to stop.
-sl | -scriptload <path>	 Loads script from path. If not specified will look for <file>.uvrtscript
-ss | -scriptsave <path>	 Saves script to path. If not specified will use <file>.uvrtscript
-sa | -saveaudio             After save, use ffmpeg to create a video with audio from source-video.
-sak| -saveaudiokeep         Don't delete no-audio video after audio save
-t  | -to <path>             Save video to path, implies -s
-tf | -tofolder <path>       Save video(s) to folder, implies -s
-ts | -timestart <time>		 Start video at <time>, given as H:MM:SS 
-ts% | -timestart% <percent> Start video at <percent> point of video, 0.0 - 100.0
-te | -timeend <time>		 End video at <time>, given as H:MM:SS 
-te% | -timeend% <percent>   End video at <percent> point of video, 0.0 - 100.0
-td | -timeduration <time>   End video at <time> after start-time
-c  | -config <config>	     Load config-file. If no extension is given .config will be added
-cr | -configreset           Reset all config values
-cs | -configset <key>=<val>[;...] Set config values on commandline, as an alternative to -c
-p  | -print                 Print current (in commandline) settings
-pl | -printline             Print current (in commandline) settings, one per line
-pv | -printverbose          Print current (in commandline) settings, with comments
-w1 | -waitone               Waits for user to press enter before continueing
-wa | -waitall               Waits for user to press enter before continueing after each vide
-we | -waiterror             Waits for user to press enter before continueing after each vide if it failed

By default <config> can be either just a name, which will be searched for first in current directory, then in app-folder, or path to config-file.
Configs does not need to set all values, in which case unset values will remain unchanged. Use -cr to reset all values
Ex: -cr -c mydefaults.config -c fisheye.config -c 640x480.config

A config-file with current settings can be created with -writeconfig. The user will be asked to confirm overwrite if it exists.
unvrtool -writeconfig <config-file> - write current config to <config-file>
Ex: unvrtool -c mydefaults -c 1920x1080 -c usesnapshots -writeconfig configs\myspecial.config
If there is a file named unvrtool.config in the same folder as the unvrtool app it will be loaded on startup and used as deafult.

Keys:
In script camera-setup mode:
Press Enter to keep fov,bo,yaw and pitch at current location.
Press Space or A to keep fov and bo, and set yaw and pitch to auto at current location.
Press Delete/Backspace to not set script command at current location, or delete if any set.
Press L / S to Load / Save uvrtscript file.

If -sc n != 0: Will skip forward automatically on enter or space.
n < 0: Will skip -n seconds, n > 0: will skip video-length/n seconds.
n = 0: Manual, user must skip back/forward manually.
Press Esc to exit mode.

General:
Left/Right keys: Skip back/forward 10 seconds. Shift: 1 frame, Ctrl: 1 sec, Alt: 1 min, 
                 Ctrl+Alt: 10 min, Ctrl+Shift: Next/Previous script-point.
Up/Down keys: Increase/Slowdown playback speed
Space: pause
Esc: exit
Enter: Disable auto mode
A: Enable auto mode
C: Toggle input channel
M: Toggle mouse mode
T: Toggle showing target markers (not a good idea while saving)
Left-mouse click: Set yaw/pitch to aim at point
Right-mouse click: Set yaw/pitch to auto
Mouse scroll-wheel: zoom fov/bo. Ctrl: fov only. Shift/Alt: bo only. Both: zoom fov/bo inverted. 

)QQ";

int main(int argc, char** argv)
{
 	util::CheckOpenCvDlls();
	std::filesystem::path appPath(argv[0]);
	string appName = appPath.filename().replace_extension().string();
	string appFolderPath = util::GetAppFolderPath();
	if (appFolderPath == "")
		appFolderPath = appPath.parent_path().string(); // Won't work if app is in $PATH
	std::filesystem::path appFolder(appFolderPath);
	string appConfigPath = (appFolder / appName).string() + ".config";

	Config cr;
	if (std::filesystem::exists(appConfigPath))
		cr.ReadFile(appConfigPath.c_str());
	Config c(cr);

	if (argc == 1)
	{
		std::cout << usage;
		cin.get();
		return -1;
	}

#define H(x) { x; continue; }

	int laststatus = 0;
	int waitif = 0;
	const char* pendingVideo = nullptr;

	VrImageFormat vr;

	for (int i = 1; i < argc+1; i++)
	{
		std::string opt;
		if (i < argc)
			opt = argv[i];
		else if (pendingVideo != nullptr)
			opt = "--process"; // trigger processing of pending video
		else
			break;

		if (opt == "--getargsfrom")
			H(if (util::GetArgsFrom(argv[++i], &argc, &argv)) i = 0;);

		if (opt == "--dbgfmtimg")
			H(c.saveDebugFormatImage = true);

		//-cr | -configreset           Reset all config values
		if (opt == "-cr" || opt == "-configreset")
			H(c = Config(cr));

		//-c  | -config <config>	     Load config-file. If no extension is given .config will be added
		if (opt == "-c" || opt == "-config")
			H(bool ok = c.ReadFile(argv[++i], appFolderPath.c_str()); std::cout << (ok ? "" : "Unable to ") << " Read " << argv[i]);
//-cs | -configset <key>=<val>[;...] Set config values on commandline, as an alternative to -c
		if (opt == "-cs" || opt == "-configset")
			H(c.ReadString(argv[++i]));


		//-p  | -print                 Print current (in commandline) settings
		if (opt == "-p" || opt == "-print")
			H(std::cout << c.Print(0) << std::endl);
		//-pl | -printline             Print current (in commandline) settings, one per line
		if (opt == "-pl" || opt == "-printline")
			H(std::cout << c.Print(1) << std::endl);
		//-pv | -printverbose          Print current (in commandline) settings, with comments
		if (opt == "-pv" || opt == "-printverbose")
			H(std::cout << c.Print(2) << std::endl);

		// -w1 | -waitone              Waits for user to press enter before continueing
		if (opt == "-w1" || opt == "-waitone")
			H(cout << "Press enter to continue"; cin.get());

		//-w  | -wait                  Waits for user to press enter before continueing
		if (opt == "-w" || opt == "-wait")
			H(waitif = 2);
		// -we | -waiterror             Waits for user to press enter before continueing if last video failed
		if (laststatus != 0 && (opt == "-we" || opt == "-waiterror"))
			H(waitif = 1);

		//-v  | -view                  View video
		if (opt == "-v" || opt == "-view")
			H(c.view = true);

		//-s  | -save                  Save video(s), without showing anything unless -g specified
		if (opt == "-s" || opt == "-save")
			H(c.save = true);

		//-sa | -saveaudio             After save, use ffmpeg to create a video with audio from source-video.
		if (opt == "-sa" || opt == "-saveaudio")
			H(c.saveaudio = true);
		//-sak | -saveaudiokeep        Don't delete no-audio video after audio save
		if (opt == "-sak" || opt == "-saveaudiokeep")
			H(c.audiokeep = true);

		//-t  | -to <path>             Save video to path, implies -s
		if (opt == "-t" || opt == "-to")
			H(c.save = true; c.outPath = argv[++i]);
		//-tf | -tofolder <path>         Save video(s) to folder, implies -s
		if (opt == "-tf" || opt == "-tofolder")
			H(c.save = true; c.outFolder = argv[++i]);

		//-sc | -script				 Allow user to setup camera shots first, esc to stop.
		if (opt == "-sc" || opt == "-script")
			H(c.scriptcam = true;);


		//-sl | -scriptload <path>	 Loads script from path. Either path to .uvrtscript file, or path to folder with <file>.uvrtscript
		if (opt == "-sl" || opt == "-scriptload")
			H(c.loadscriptPath = std::string(argv[++i]));

		//-ss | -scriptsave <path>	 Saves script to path
		if (opt == "-ss" || opt == "-scriptsave")
			H(c.savescriptPath = std::string(argv[++i]));

		//-ts | -timestart <time>		 Start video at <time>, given as H:MM:SS 
		if (opt == "-ts" || opt == "-timestart")
			H(c.timeStartSec = TimeCodeHMS(argv[++i]).ToSecs());

		// -ts% | -timestart% <percent> Start video at <percent> point of video, 0.0 - 100.0
		if (opt == "-ts%" || opt == "-timestart%")
			H(c.timeStartPrc = stof(argv[++i]));

		//-te | -timeend <time>		 End video at <time>, given as H:MM:SS 
		if (opt == "-te" || opt == "-timeend")
			H(c.timeEndSec = TimeCodeHMS(argv[++i]).ToSecs());

		if (opt == "-te%" || opt == "-timeend%")
			H(c.timeEndPrc = stof(argv[++i]));

		// -td | -timeduration <time>   End video at <time> after start - time
		if (opt == "-td" || opt == "-timeduration")
			H(c.timeDurationSec = TimeCodeHMS(argv[++i]).ToSecs());

		//-if | -inputformat <format>  Specify video format or auto (default). Ex: lr:180:fisheye or tb:360:equirectangular
		if (opt == "-if" || opt == "-inputformat")
			H(vr = VrImageFormat::Parse(argv[++i]));

		// unvrtool -writeconfig <config-file> - write current config to <config-file>
		if (opt == "-writeconfig")
		{
			if (argc == ++i)
			{
				auto confPath = appName + ".config";
				if (std::filesystem::exists(appConfigPath))
				{
					std::cout << "Press enter to overwrite " << confPath << " or Ctrl+C to abort" << std::endl;
					cin.get();
				}
				std::cout << "Writing " << confPath << std::endl;
				H(c.Write(confPath.c_str()));
			}
			else
				H(c.Write(argv[i]));
		}

		//<file> or -i <file>          Video to open
		if (opt == "--process" || opt == "-i" || opt[0] != '-')
		{
			if (pendingVideo != nullptr)
			{
				int s = 0;
				if (std::filesystem::exists(pendingVideo))
				{
					VrRecorder v(c);
					v.videopath = std::string(pendingVideo);
					s = v.Run(vr);
					cout << std::endl;
				}
				else
				{
					s = 1;
					cout << "Not Found: " << pendingVideo << std::endl;
				}
				pendingVideo = nullptr;

				c.outPath = ""; // Reset, if set
				if (s != 0 && waitif > 0)
				{
					cout << "Error, press enter to continue"; cin.get();
				}
				else if (waitif > 1)
				{
					cout << "Press enter to continue"; cin.get();
				}
				cout << std::endl;
			}
			if (opt != "--process")
				if (opt == "-i")
					pendingVideo = argv[++i];
				else
				{
					std::string filepath(argv[i]);
					bool unvrFile = filepath.find(".unvr.") != std::string::npos;
					if (!unvrFile)
						pendingVideo = argv[i];
					else
						cout << "Ignoring unvr file " << argv[i] << std::endl;
				}
		}
	}
}

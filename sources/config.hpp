//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "simpleconfig.hpp"
#include "vrimageformat.hpp"

class Config : public SimpleConfig
{
private:
	const char* DefaultConfig = R"QQ(# Unvrtool config file

############# Fundamentals #############

# Video output size
#Width = 1280
#Height = 720
Width = 1920
Height = 1080

# FOV: Usually 45 - 70. 45 gives nice projection, but might clip to much. 65 will captures more, but gives more widelense effect
Fov = 65.0
FovMin = 1.0
#FovMax = 89.0
FovMax = 130.0

# BackOff: Usually 0 - 70. 0 will give the most convincing projection, but might require a higher Fov to compensate. BackOff around 20-50 will allow lower Fov while capturing a bigger area
BackOff = 50.0
BackOffMin = 0
BackOffMax = 100

# Max limits on pitch (y) and yaw (x)
# Negative values are calculated from border, MaxPitch=-40 -> MaxPitch=90-40=50
# For 360 deg video, MaxYaw=-75 -> MaxYaw = 180-75 = 105
#MaxPitch = 50
#MaxYaw = -50
MaxPitch = 80
MaxYaw = 80

# How many seconds to do camera parameter changes over, for smooth pans and zooms
CrossFadeSecs = 4.0

#How many additional times autodetect needs to verify
AutodetectConfirmations = 0

# Path to ffmpeg
ffmpegPath = ffmpeg.exe

############# Output video #############

#OutFOURCC describes which codec the output video should be encoded with, like mp4v hvc1 XVID MP42 X264
OutFOURCC = mp4v

#OutExt sets default output video filename extension
OutExt = .mp4

#OutQuality specifies quality (0..100%) of the encoded videostream
#OutQuality = -1

############# Tracking #############

#TrackAverageFrames how many frames to average tracking over. Too few and camera gets jumpy, too many and it will be slow to respond to change
TrackAverageFrames = 100

#CenterAmp: Usually 0.5 - 2. If > 1 will weight motion in center more, < 1 will weight motion in the edges more
TrackCenterAmp = 1.0

# OffAmp: Usually 0 - 1. Can enhance or reduce camera-motion towards edges. Set < 1 to keep camera more in the center. Useful with a high FOV
TrackXOffAmp = 1.0
TrackYOffAmpUp = 1.0
TrackYOffAmpDown = 1.0

# Tracking max limits on pitch (y) and yaw (x)
TrackMaxPitch = 40
TrackMaxYaw = -60


############# Snapshots #############

# Don't take snapshots
#SecsPerSnapshot = 0
# Take a snapshot every second minute
SecsPerSnapshot = 120

# Save all snapshots
#SaveSnapshots = All
# Save only first two snapshots
#SaveSnapshots = 2
# Save no snapshots
SaveSnapshots = 0

# Thumbnails: Set either to 0 or -1 to disable
#ThumbnailsImageWidth = 640
#ThumbnailsSheetWidth = 1920

ThumbnailsImageWidth = 0
ThumbnailsSheetWidth = 1920
)QQ";


public:

	Config()
	{
		std::stringstream ss(DefaultConfig);
		Read(ss);
		keepBlankLines = false;
	}

#define AccINT(name) int get ## name () { return GetInt(#name); } void set ## name (int val) {Set(#name, std::to_string(val)); }

	AccINT(Width);
	AccINT(Height);

#undef AccINT

	VrImageFormat vrFormat;

	// Commandline options
	bool view = false;
	bool save = false;
	bool saveaudio = false;
	bool audiokeep = false;
	bool scriptcam = false;

	std::string outPath = "";
	std::string outFolder = "";

	std::string loadscriptPath = "";
	std::string savescriptPath = "";

};


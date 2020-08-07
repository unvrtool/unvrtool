# unvrtool
View and save virtual reality videos projected to flat displays automatically with selection algorithm

Autodetects and supports
* 360 / 180 Equirectangular LR/TB/Mono 
* 180 Fisheye (aka Orthographic/Spherical) LR
* Stereoscopic LR/TB

Features:
* Automatic or manual camera control
* Scripting of camera, including pitch, yaw, fov and backoff
* Save whole or part of video as mp4
* Autodetect or manual specification of VR format
* Options to create snapshots and sheet of snapshots
* All reasonable resolutions of input and output video should be supported


Limitations:
* No sound while playing, but has option to add sound after saving

 

Currently this is a windows-only project, but the aim is to make it multi-platform. With that in mind OpenGl is used insted of DirectX, and all of the other dependencies should be multi-platform as well.


### License
Licensed with 3-clause BSD License, see LICENSE.txt


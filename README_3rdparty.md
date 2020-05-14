3rdparty dependencies:

* OpenCV (4.3.0)
Available at https://opencv.org/releases (get the Windows binaries)
Direct link: https://sourceforge.net/projects/opencvlibrary/files/4.3.0/opencv-4.3.0-vc14_vc15.exe/download

Extract so that opencv/build is located at <unvrtool>\3rdparty\opencv\build\
Add environment path <unvrtool>\3rdparty\opencv\build\x64\vc15\bin\
OR
copy dll and pdb files of above folder into your executabe directory.

(executabe directory is <unvrtool>\Debug\ and <unvrtool>\Release\ )

* GLAD
Available at https://glad.dav1d.de/
Just generate at all defaults and unzip at <unvrtool>\3rdparty\glad

* GLFW (3.3.2)
Available at https://www.glfw.org/download.html (get the 64-bit Windows binaries)
Direct link: https://github.com/glfw/glfw/releases/download/3.3.2/glfw-3.3.2.bin.WIN64.zip

Extract so that lib-vc2019 is located at <unvrtool>\3rdparty\glfw\lib-vc2019

* GLM
Available at https://github.com/g-truc/glm/releases
Extract so that glm.hpp is located at <unvrtool>\3rdparty\glm\glm\glm.hpp

* FFMPEG (optionally, for video-manipulation such as adding audio-track from source to unvrtool-saved video)
Available at https://ffmpeg.zeranoe.com/builds (get the 4.x.x version Windows 64-bit Static)
Extract and copy ffmpeg.exe to your executable directory.




OpenCV is very big and therefore not included, so you have to download it.

It can be downloaded from https://opencv.org/releases (get the Windows binaries, 4.3.0 or higher) or
downloaded directly from https://sourceforge.net/projects/opencvlibrary/files/4.3.0/opencv-4.3.0-vc14_vc15.exe/download

Extract so that opencv/build is located at <unvrtoolpath>\3rdparty\opencv\build\
Add environment path <unvrtoolpath>\3rdparty\opencv\build\x64\vc15\bin\
OR
copy dll and pdb files of above folder into your executabe directory.

(executabe directory is <unvrtoolpath>\Debug\ and/or <unvrtoolpath>\Release\ )


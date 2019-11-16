setlocal
set ARCH=%1
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" %ARCH%
md %ARCH%
cd %ARCH%
md qtbase
cd qtbase
call C:\src\qtbase\configure -opensource -confirm-license -prefix C:\Qt\%ARCH% -opengl desktop -release -nomake tests -nomake examples
jom
jom install
cd ..
md qttools\src\windeployqt
cd qttools\src\windeployqt
C:\Qt\%ARCH%\bin\qmake C:\src\qttools\src\windeployqt\windeployqt.pro
jom
jom install
cd ..
md qtscript
cd qtscript
C:\Qt\%ARCH%\bin\qmake C:\src\qtscript\qtscript.pro
jom
jom install
endlocal

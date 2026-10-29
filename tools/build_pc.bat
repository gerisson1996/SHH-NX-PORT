@echo off
rem Builds the game for the PC (out\pc\shh.exe), with ReXGlue's own graphics.
rem Needs the codegen to have run first (app\generated). Same requirements as build_host.bat.
rem Run it from cmd or PowerShell, not from an MSYS shell.
setlocal
set ROOT=%~dp0..
set VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
if defined VSINSTALL set VS=%VSINSTALL%
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
rem shh-nx (2026-10-28): the shader steps run Python. The one on PATH here is devkitPro's MSYS2 python, which cannot
rem read Windows paths like C:/dev/... (it looked for /c/dev/...): name a native Windows one.
set PYTHON=%SHH_PYTHON%
if "%PYTHON%"=="" set PYTHON=C:\msys64\mingw64\bin\python.exe
if not exist "%PYTHON%" set PYTHON=python
cmake -S "%ROOT%\app" -B "%ROOT%\out\pc" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DREXGLUE_ENABLE_TRACY=OFF ^
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 ^
  -DREXSDK_DIR="%ROOT%\sdk" -DPython3_EXECUTABLE="%PYTHON%" -DPYTHON_EXECUTABLE="%PYTHON%" || exit /b 1
cmake --build "%ROOT%\out\pc" --target shh || exit /b 1

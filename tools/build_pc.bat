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
cmake -S "%ROOT%\app" -B "%ROOT%\out\pc" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DREXGLUE_ENABLE_TRACY=OFF ^
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 ^
  -DREXSDK_DIR="%ROOT%\sdk" || exit /b 1
cmake --build "%ROOT%\out\pc" --target shh || exit /b 1

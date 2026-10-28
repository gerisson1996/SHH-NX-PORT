@echo off
rem Builds the rexglue code generator for the PC (out\host\rexglue.exe).
rem Needs Visual Studio 2022 Build Tools (Windows SDK, CMake, Ninja) and LLVM clang 18+.
rem Run it from cmd or PowerShell, not from an MSYS shell: MSYS cmake does not understand Windows paths.
setlocal
set ROOT=%~dp0..
set VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
if defined VSINSTALL set VS=%VSINSTALL%
set PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Program Files\LLVM\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
cmake -S "%ROOT%\sdk" -B "%ROOT%\out\host" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DREXGLUE_ENABLE_TRACY=OFF ^
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_STANDARD=23 || exit /b 1
cmake --build "%ROOT%\out\host" --target rexglue || exit /b 1

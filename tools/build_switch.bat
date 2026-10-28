@echo off
rem Builds the Switch NRO (out\switch\shh.nro).
rem Needs devkitPro (devkitA64, libnx), the codegen (app\generated) and the static NVK driver SDK, the
rem portlibs/switch folder with lib\libvulkan.a built from mesa-switch (see mesa/README.md in nfsmw-nx).
rem
rem   tools\build_switch.bat C:\path\to\nvk_sdk
rem
rem Run it from cmd or PowerShell, not from an MSYS shell.
setlocal
set ROOT=%~dp0..
set NVK_SDK=%~1
if "%NVK_SDK%"=="" set NVK_SDK=%SHH_NVK_SDK%
if "%NVK_SDK%"=="" (
  echo usage: tools\build_switch.bat ^<NVK SDK folder with lib\libvulkan.a^>
  exit /b 1
)
set VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
if defined VSINSTALL set VS=%VSINSTALL%
rem devkitPro sets DEVKITPRO=/opt/devkitpro (MSYS form), which CMake on Windows cannot use.
set DEVKITPRO=C:/devkitPro
set NINJA=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe
rem glslang runs Python at configure time; cross-compiling, CMake does not look for it on the PC.
set PYTHON=%SHH_PYTHON%
if "%PYTHON%"=="" set PYTHON=C:\msys64\mingw64\bin\python.exe
set PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
if not exist "%ROOT%\out\switch\CMakeCache.txt" (
  cmake -S "%ROOT%\app" -B "%ROOT%\out\switch" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%ROOT%\tools\switch\cmake\switch-devkitA64.cmake" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" -DDEVKITPRO=C:/devkitPro -DPYTHON_EXECUTABLE="%PYTHON%" ^
    -DPython3_EXECUTABLE="%PYTHON%" ^
    -DREXSDK_DIR="%ROOT%\sdk" -DREXGLUE_SWITCH_NVK_SDK="%NVK_SDK%" || exit /b 1
)
cmake --build "%ROOT%\out\switch" --target shh || exit /b 1

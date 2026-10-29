@echo off
rem Builds the Switch NRO.
rem Needs devkitPro (devkitA64, libnx, deko3d), the codegen (app\generated) and the static NVK driver SDK, the
rem portlibs/switch folder with lib\libvulkan.a built from mesa-switch (see mesa/README.md in nfsmw-nx). A nonvk build
rem still reads the SDK's Vulkan headers, but links no driver.
rem
rem   tools\build_switch.bat C:\path\to\nvk_sdk          out\switch\shh.nro (emulated Xenos GPU on NVK, or Renut)
rem   tools\build_switch.bat C:\path\to\nvk_sdk nonvk    out\switch-deko\shh.nro (Renut on deko3d only; stage 4)
rem
rem Renut needs the shader package from the PC first (docs/plano-renut.md, "Etapa 4"): app\out\renut_switch.
rem Run it from cmd or PowerShell, not from an MSYS shell.
setlocal
set ROOT=%~dp0..
set NVK_SDK=%~1
if "%NVK_SDK%"=="" set NVK_SDK=%SHH_NVK_SDK%
if "%NVK_SDK%"=="" (
  echo usage: tools\build_switch.bat ^<NVK SDK folder with lib\libvulkan.a^> [nonvk]
  exit /b 1
)
set BUILD=%ROOT%\out\switch
set NO_NVK=OFF
if /i "%~2"=="nonvk" (
  set BUILD=%ROOT%\out\switch-deko
  set NO_NVK=ON
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
if not exist "%BUILD%\CMakeCache.txt" (
  cmake -S "%ROOT%\app" -B "%BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_TOOLCHAIN_FILE="%ROOT%\tools\switch\cmake\switch-devkitA64.cmake" ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" -DDEVKITPRO=C:/devkitPro -DPYTHON_EXECUTABLE="%PYTHON%" ^
    -DPython3_EXECUTABLE="%PYTHON%" ^
    -DREXSDK_DIR="%ROOT%\sdk" -DREXGLUE_SWITCH_NVK_SDK="%NVK_SDK%" || exit /b 1
)
cmake "%BUILD%" -DREXGLUE_SWITCH_NO_NVK=%NO_NVK% -DSHH_RENUT_RENDERER=ON > nul || exit /b 1
cmake --build "%BUILD%" --target shh || exit /b 1

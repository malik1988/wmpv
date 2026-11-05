@echo off
setlocal EnableExtensions EnableDelayedExpansion
set "PROJ=%~dp0"
set "PROJ=%PROJ:~0,-1%"

rem ===========================================================================
rem  wmpv 一键构建（可移植）
rem  - 工具路径可用环境变量覆盖：WMPV_VCVARS / WMPV_LLVM_BIN /
rem    WMPV_CMAKE / WMPV_NINJA；未设置时自动探测，脚本里没有绝对路径。
rem  - 依赖路径由 cmake/config.cmake 的配置变量决定，默认相对工程目录。
rem  - 额外参数原样透传给 cmake，例如：
rem        build.bat -DWMPV_MPV_ROOT=D:/work/mpv
rem        build.bat -C cmake\local.cmake
rem ===========================================================================

rem --- [1/4] 定位工具链 ------------------------------------------------------
call :locate_tools
if errorlevel 1 exit /b 1

rem --- [2/4] 图标（仅缺失时生成） --------------------------------------------
if not exist "%PROJ%\assets\icon.ico" (
    if not exist "%PROJ%\assets" mkdir "%PROJ%\assets"
    echo [2/4] Generating icon...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJ%\tools\make_icon.ps1" -Out "%PROJ%\assets\icon.ico"
    if not exist "%PROJ%\assets\icon.ico" ( echo ERROR: icon generation failed & exit /b 1 )
) else (
    echo [2/4] Icon already present, skipping.
)

rem --- [3/4] 配置 + 编译 -----------------------------------------------------
echo [3/4] Building with CMake + Ninja (clang-cl / lld-link, static mpv)...
call "%WMPV_VCVARS%" x64
if errorlevel 1 ( echo ERROR: vcvarsall.bat failed & exit /b 1 )

for %%I in ("%WMPV_CMAKE%") do set "CMAKE_DIR=%%~dpI"
for %%I in ("%WMPV_NINJA%") do set "NINJA_DIR=%%~dpI"
set "PATH=%WMPV_LLVM_BIN%;%CMAKE_DIR%;%NINJA_DIR%;%PATH%"

if not exist "%PROJ%\build" mkdir "%PROJ%\build"
"%WMPV_CMAKE%" -S "%PROJ%" -B "%PROJ%\build" -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    "-DWMPV_LLVM_BIN=%WMPV_LLVM_BIN%" ^
    %*
if errorlevel 1 ( echo ERROR: CMake configure failed & exit /b 1 )

"%WMPV_CMAKE%" --build "%PROJ%\build" --config Release
if errorlevel 1 ( echo ERROR: build failed & exit /b 1 )

rem --- [4/4] 安装 ------------------------------------------------------------
echo [4/4] Installing Release to %PROJ%\install ...
"%WMPV_CMAKE%" --install "%PROJ%\build" --config Release --prefix "%PROJ%"
if errorlevel 1 ( echo ERROR: install failed & exit /b 1 )

echo.
echo Build complete: %PROJ%\install\wmpv.exe  (statically linked -- single file)
endlocal
exit /b 0

rem ===========================================================================
:locate_tools
rem -- 优先用 PATH 里已有的工具 --
if not defined WMPV_CMAKE (
    for /f "delims=" %%I in ('where cmake 2^>nul') do if not defined WMPV_CMAKE set "WMPV_CMAKE=%%I"
)
if not defined WMPV_NINJA (
    for /f "delims=" %%I in ('where ninja 2^>nul') do if not defined WMPV_NINJA set "WMPV_NINJA=%%I"
)

rem -- 用 vswhere 定位任意版本的 Visual Studio（不写死版本/盘符） --
set "VSROOT="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "delims=" %%I in ('"%VSWHERE%" -latest -products * -property installationPath 2^>nul') do set "VSROOT=%%I"
)
if defined VSROOT (
    if not defined WMPV_VCVARS if exist "!VSROOT!\VC\Auxiliary\Build\vcvarsall.bat" set "WMPV_VCVARS=!VSROOT!\VC\Auxiliary\Build\vcvarsall.bat"
    if not defined WMPV_LLVM_BIN if exist "!VSROOT!\VC\Tools\Llvm\x64\bin\clang-cl.exe" set "WMPV_LLVM_BIN=!VSROOT!\VC\Tools\Llvm\x64\bin"
    if not defined WMPV_CMAKE if exist "!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "WMPV_CMAKE=!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if not defined WMPV_NINJA if exist "!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" set "WMPV_NINJA=!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
)

rem -- LLVM 也可以直接来自 PATH --
if not defined WMPV_LLVM_BIN (
    for /f "delims=" %%I in ('where clang-cl 2^>nul') do if not defined WMPV_LLVM_BIN for %%J in ("%%I") do set "WMPV_LLVM_BIN=%%~dpJ"
)
if defined WMPV_LLVM_BIN (
    for %%I in ("%WMPV_LLVM_BIN%") do set "WMPV_LLVM_BIN=%%~fI"
)

if "%WMPV_VCVARS%"=="" ( echo ERROR: vcvarsall.bat not found. Set WMPV_VCVARS to its full path. & exit /b 1 )
if not exist "%WMPV_VCVARS%" ( echo ERROR: vcvarsall.bat not found at %WMPV_VCVARS% & exit /b 1 )
if not exist "%WMPV_LLVM_BIN%\clang-cl.exe" ( echo ERROR: clang-cl not found. Set WMPV_LLVM_BIN to LLVM's bin directory. & exit /b 1 )
if not exist "%WMPV_CMAKE%" ( echo ERROR: cmake not found. Set WMPV_CMAKE to cmake.exe full path. & exit /b 1 )
if not exist "%WMPV_NINJA%" ( echo ERROR: ninja not found. Set WMPV_NINJA to ninja.exe full path. & exit /b 1 )

echo [1/4] Toolchain:
echo        VS     : %WMPV_VCVARS%
echo        LLVM   : %WMPV_LLVM_BIN%
echo        CMake  : %WMPV_CMAKE%
echo        Ninja  : %WMPV_NINJA%
exit /b 0

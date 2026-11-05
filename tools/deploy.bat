@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem PROJ = parent directory of this script's directory (i.e. project root)
for %%I in ("%~dp0..") do set "PROJ=%%~fI"

rem 可用 WMPV_CMAKE 环境变量指定 cmake.exe；否则依次从 PATH、VS 自带 CMake 中探测
if defined WMPV_CMAKE set "CMAKE_EXE=%WMPV_CMAKE%"
if not defined CMAKE_EXE (
    for /f "delims=" %%I in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"
)
if not defined CMAKE_EXE (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "delims=" %%I in ('"%VSWHERE%" -latest -products * -property installationPath 2^>nul') do set "VSROOT=%%I"
    )
    if defined VSROOT (
        if exist "!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" set "CMAKE_EXE=!VSROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
)
if not defined CMAKE_EXE (
    echo ERROR: cmake.exe not found. Set WMPV_CMAKE to its full path.
    exit /b 1
)

if not exist "%PROJ%\build\wmpv.exe" (
    echo ERROR: build\wmpv.exe not found -- run build.bat first.
    exit /b 1
)

echo Installing wmpv (Release) to %PROJ%\install ...
"%CMAKE_EXE%" --install "%PROJ%\build" --config Release --prefix "%PROJ%"
if errorlevel 1 ( exit /b 1 )

echo(
echo Done. Shippable artifacts: %PROJ%\install\wmpv.exe  (+ icon.ico)
echo NOTE: wmpv.exe is statically linked and self-contained -- ship it alone.
echo       The only external runtime needed is vulkan-1.dll (from your GPU driver)
echo       plus the Windows 11 Universal CRT. No side-by-side DLLs required.

endlocal

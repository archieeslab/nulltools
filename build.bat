@echo off
setlocal enabledelayedexpansion

where cl >nul 2>nul
if errorlevel 1 (
  set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
  )
  if defined VSDIR (
    call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
  ) else (
    echo no visual studio c++ toolchain found, falling back to whatever compiler is on PATH
  )
)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1
cmake --build build
if errorlevel 1 exit /b 1

echo.
echo built:
dir /b build\bin\*.exe

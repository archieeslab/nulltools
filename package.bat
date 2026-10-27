@echo off
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

if not exist "%~dp0dist" mkdir "%~dp0dist"
copy /y "%~dp0build\bin\*.exe" "%~dp0dist\" >nul

echo.
echo packaged:
dir /b "%~dp0dist\*.exe"

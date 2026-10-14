@echo off
rem Builds ControllerWorkshopPlugin.xex (DashLaunch system plugin). Usage: build_plugin.bat [Release|Debug]
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
set OUT=%~dp0%CONFIG%
set IMAGEXEX="C:\Program Files (x86)\Microsoft Xbox 360 SDK\bin\win32\imagexex.exe"
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86 >nul

rem 1. Compile + link the DLL (MSBuild also makes a throwaway title XEX).
"%WINDIR%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe" "%~dp0ControllerWorkshopPlugin.vcxproj" /nologo /v:minimal /t:Rebuild /p:Configuration=%CONFIG% /p:Platform="Xbox 360"
if errorlevel 1 exit /b 1

rem 2. Mark the DLL 4 KB-aligned so imagexex can place it in system memory.
python "%~dp0..\..\tools\pe_4k_align.py" "%OUT%\ControllerWorkshopPlugin.dll"
if errorlevel 1 exit /b 1

rem 3. Build the system-DLL XEX (sysdll, 0x91E80000, 4 KB pages; see xex.xml).
rem    Leave it dev-kit encrypted exactly as imagexex writes it; editing it breaks the header hash.
%IMAGEXEX% /NOLOGO /IN:"%OUT%\ControllerWorkshopPlugin.dll" /OUT:"%OUT%\ControllerWorkshopPlugin.xex" /CONFIG:"%~dp0xex.xml"
if errorlevel 1 exit /b 1

if not exist "%~dp0..\..\dist\ControllerWorkshop" mkdir "%~dp0..\..\dist\ControllerWorkshop"
copy /y "%OUT%\ControllerWorkshopPlugin.xex" "%~dp0..\..\dist\ControllerWorkshop\ControllerWorkshopPlugin.xex" >nul
echo Built %~dp0..\..\dist\ControllerWorkshop\ControllerWorkshopPlugin.xex

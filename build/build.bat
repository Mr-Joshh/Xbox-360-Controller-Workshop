@echo off
rem Builds Controller Workshop's default.xex. Usage: build.bat [Release|Debug]
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86 >nul
"%WINDIR%\Microsoft.NET\Framework\v4.0.30319\MSBuild.exe" "%~dp0ControllerWorkshop.vcxproj" /nologo /v:minimal /t:Rebuild /p:Configuration=%CONFIG% /p:Platform="Xbox 360"
if errorlevel 1 exit /b 1
if not exist "%~dp0..\dist\ControllerWorkshop" mkdir "%~dp0..\dist\ControllerWorkshop"
copy /y "%~dp0%CONFIG%\ControllerWorkshop.xex" "%~dp0..\dist\ControllerWorkshop\default.xex" >nul
echo Built %~dp0..\dist\ControllerWorkshop\default.xex

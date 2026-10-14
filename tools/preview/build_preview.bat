@echo off
rem Builds and runs the PC layout preview, then renders the SVGs to PNG with headless Edge.
call "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\vcvarsall.bat" x86 >nul
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /EHsc /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /I inc /Feout\preview.exe /Foout\ preview.cpp render_svg.cpp ..\..\src\shape.cpp ..\..\src\remap.cpp ..\..\src\games.cpp ..\..\src\remap_ui.cpp ..\..\src\theme.cpp battery_stub.cpp system_stub.cpp user32.lib || exit /b 1
cd out
.\preview.exe || exit /b 1
.\preview.exe light || exit /b 1
set EDGE="C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
for %%f in (*.svg) do %EDGE% --headless --disable-gpu --hide-scrollbars --window-size=1280,720 --screenshot="%~dp0out\%%~nf.png" "file:///%~dp0out\%%f" 2>nul

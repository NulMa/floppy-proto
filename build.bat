@echo off
setlocal enabledelayedexpansion
set ROOT=%~dp0
set SRC=%ROOT%vendor\raylib-5.5\src
set RLIB=%ROOT%vendor\raylib-mt\raylib_mt.lib

if not exist "%RLIB%" (
    echo [INFO] raylib_mt.lib not found - building raylib from source first...
    call "%ROOT%build-raylib.bat"
    if errorlevel 1 exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist "%ROOT%build" mkdir "%ROOT%build"

cl /nologo /O2 /Oi /MT /Gy /GS- /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS /I"%SRC%" ^
   "%ROOT%src\main.c" "%ROOT%src\sim.c" ^
   /Fe:"%ROOT%build\floppy.exe" /Fo:"%ROOT%build\\" ^
   /link /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
   "%RLIB%" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
if errorlevel 1 exit /b 1

copy /y "%ROOT%tune.txt" "%ROOT%build\tune.txt" >nul

for %%A in ("%ROOT%build\floppy.exe") do set SZ=%%~zA
set /a PCT=!SZ! * 100 / 1474560
echo.
echo   build\floppy.exe   !SZ! bytes   ^(limit 1,474,560^)   -   !PCT!%% of budget
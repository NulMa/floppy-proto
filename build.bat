@echo off
setlocal enabledelayedexpansion
set ROOT=%~dp0
set SRC=%ROOT%vendor\raylib-5.5\src
set RLIB=%ROOT%vendor\raylib-mt\raylib_mt.lib

if not exist "%SRC%\rcore.c" (
    echo [INFO] raylib source missing - run fetch-deps.bat first
    exit /b 1
)
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

rem 실행 파일 아이콘. art\icon.ico 가 없으면 tools\make-icon.ps1 로 만든다
rem (원본은 art\floppy.png 0번 프레임 — 캐릭터를 다시 그리면 이것도 다시 돌린다)
if not exist "%ROOT%art\icon.ico" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\make-icon.ps1"
)
rc /nologo /fo "%ROOT%build\app.res" "%ROOT%src\app.rc"
if errorlevel 1 exit /b 1

cl /nologo /O2 /Oi /MT /Gy /GS- /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS /I"%SRC%" ^
   "%ROOT%src\main.c" "%ROOT%src\sim.c" "%ROOT%src\levels.c" "%ROOT%src\progress.c" "%ROOT%src\sfx.c" "%ROOT%src\bgm.c" ^
   /Fe:"%ROOT%build\floppy.exe" /Fo:"%ROOT%build\\" ^
   /link /OPT:REF /OPT:ICF /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup ^
   "%ROOT%build\app.res" ^
   "%RLIB%" opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
if errorlevel 1 exit /b 1

copy /y "%ROOT%tune.txt" "%ROOT%build\tune.txt" >nul

for %%A in ("%ROOT%build\floppy.exe") do set SZ=%%~zA
set /a PCT=!SZ! * 100 / 1474560
echo.
echo   build\floppy.exe   !SZ! bytes   ^(limit 1,474,560^)   -   !PCT!%% of budget
@echo off
setlocal
set ROOT=%~dp0..\
set SECS=%1
if "%SECS%"=="" set SECS=40

where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist "%ROOT%build" mkdir "%ROOT%build"
rem separate obj dir: bgm.obj here is built against the stub, must not reach the game link
if not exist "%ROOT%build\prev" mkdir "%ROOT%build\prev"

rem stub\raylib.h must come first so it shadows the real raylib.h
cl /nologo /O2 /MT /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS ^
   /I"%ROOT%tools\stub" /I"%ROOT%src" ^
   "%ROOT%tools\bgm-preview.c" "%ROOT%src\bgm.c" ^
   /Fe:"%ROOT%build\bgm-preview.exe" /Fo:"%ROOT%build\prev\\" >nul
if errorlevel 1 (
    echo [FAIL] compile
    exit /b 1
)

pushd "%ROOT%"
"%ROOT%build\bgm-preview.exe" %SECS%
popd

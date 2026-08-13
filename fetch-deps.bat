@echo off
REM raylib 5.5 소스를 vendor\ 로 받는다.
REM vendor\ 는 90MB 라 저장소에 넣지 않는다. 새로 클론했으면 이걸 먼저 돌린다.
setlocal
set ROOT=%~dp0
set VEN=%ROOT%vendor

if exist "%VEN%\raylib-5.5\src\rcore.c" (
    echo raylib source already present - nothing to do.
    exit /b 0
)

if not exist "%VEN%" mkdir "%VEN%"
echo Downloading raylib 5.5 source...
powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip' -OutFile '%VEN%\raylib-src.zip' -UseBasicParsing"
if errorlevel 1 exit /b 1

echo Extracting (this takes a minute)...
powershell -NoProfile -Command "Expand-Archive -Path '%VEN%\raylib-src.zip' -DestinationPath '%VEN%' -Force"
if errorlevel 1 exit /b 1

if exist "%VEN%\raylib-5.5\src\rcore.c" (
    echo OK. Now run build.bat
) else (
    echo [ERROR] extraction did not produce vendor\raylib-5.5\src
    exit /b 1
)
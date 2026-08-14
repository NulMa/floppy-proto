@echo off
REM raylib 5.5 ? /MT (?? CRT) ? ?? ????.
REM ?? ?? lib ? /MD ? ??? ??? VC++ ??? ??? ??? ???.
REM ??? ?? exe ? ???? ? ??? ???.
setlocal
set ROOT=%~dp0
set SRC=%ROOT%vendor\raylib-5.5\src
set OUT=%ROOT%vendor\raylib-mt

if not exist "%SRC%\rcore.c" (
    echo [ERROR] raylib source not found - run fetch-deps.bat first
    exit /b 1
)

REM ? ?? ??? ???? ?? (OGG/MP3/QOA/MOD).
REM vendor ? gitignore ? fetch-deps ? ?? ??? ????? ?? ???.
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\trim-raylib-audio.ps1" -ConfigPath "%SRC%\config.h"

where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"

REM /Gy = ?? ?? ??. ?? ??? ??? ? ?? ??? ??? ???.
REM /GS- = ?? ?? ?? ??. ??? ?? ??.
cl /nologo /c /O2 /Oi /MT /Gy /GS- /W0 /utf-8 ^
   -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_33 -D_CRT_SECURE_NO_WARNINGS ^
   -I"%SRC%" -I"%SRC%\external\glfw\include" -I"%SRC%\external\glfw\deps" ^
   "%SRC%\rcore.c" "%SRC%\rshapes.c" "%SRC%\rtextures.c" "%SRC%\rtext.c" ^
   "%SRC%\rmodels.c" "%SRC%\raudio.c" "%SRC%\utils.c" "%SRC%\rglfw.c"
if errorlevel 1 ( popd & exit /b 1 )

lib /nologo /OUT:"%OUT%\raylib_mt.lib" *.obj
if errorlevel 1 ( popd & exit /b 1 )
popd

for %%A in ("%OUT%\raylib_mt.lib") do echo   raylib_mt.lib  %%~zA bytes  ^(static CRT^)
@echo off
setlocal
set ROOT=%~dp0
where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)
if not exist "%ROOT%build\obj_test" mkdir "%ROOT%build\obj_test"
cl /nologo /O2 /MT /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS ^
   "%ROOT%src\test.c" "%ROOT%src\sim.c" ^
   /Fe:"%ROOT%build\test.exe" /Fo:"%ROOT%build\obj_test\\" >nul
if errorlevel 1 exit /b 1
"%ROOT%build\test.exe"
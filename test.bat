@echo off
setlocal
set ROOT=%~dp0
where cl >nul 2>nul
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
)
if not exist "%ROOT%build\obj_test" mkdir "%ROOT%build\obj_test"
rem ★ 컴파일 출력을 로그로 받는다. 예전엔 >nul 로 통째로 버렸는데,
rem    그러면 컴파일이 깨졌을 때 **아무것도 안 찍히고 그냥 종료**한다.
rem    2026-08-19 에 test.c 가 깨졌을 때 원인을 찾는 데 한참 걸렸다.
rem    성공하면 조용하고, 실패하면 로그를 그대로 보여준다.
cl /nologo /O2 /MT /W3 /utf-8 -D_CRT_SECURE_NO_WARNINGS ^
   "%ROOT%src\test.c" "%ROOT%src\sim.c" "%ROOT%src\levels.c" ^
   /Fe:"%ROOT%build\test.exe" /Fo:"%ROOT%build\obj_test\\" >"%ROOT%build\test_build.log" 2>&1
if errorlevel 1 (
    echo [test] 컴파일 실패:
    type "%ROOT%build\test_build.log"
    exit /b 1
)
"%ROOT%build\test.exe"

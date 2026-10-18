@echo off
rem ===========================================================================
rem  Migration Switcher - ビルド エントリ（ツールチェーンを自動判定します）
rem
rem    build.bat            ツールチェーンを自動判定してビルド
rem    build.bat msvc       Visual Studio 2022 (cl.exe) でビルド
rem    build.bat mingw      MinGW-w64 / MSYS2 (g++) でビルド
rem    build.bat test       単体テストをビルドして実行
rem    build.bat clean      生成物を削除
rem    build.bat help       この使い方を表示
rem
rem  ソースは Shift_JIS (CP932) / 改行 CRLF です。
rem ===========================================================================
setlocal
chcp 932 >nul
set "ROOT=%~dp0"
set "MODE=%~1"

if /i "%MODE%"=="msvc"  goto :do_msvc
if /i "%MODE%"=="mingw" goto :do_mingw
if /i "%MODE%"=="test"  goto :do_test
if /i "%MODE%"=="clean" goto :do_clean
if /i "%MODE%"=="help"  goto :usage
if /i "%MODE%"=="-h"    goto :usage
if /i "%MODE%"=="--help" goto :usage
if not "%MODE%"=="" goto :usage

rem ---- ツールチェーンの自動判定（Visual Studio を優先）----
call :detect_msvc
if "%HAVE_MSVC%"=="1" goto :do_msvc

call :detect_mingw
if "%HAVE_MINGW%"=="1" goto :do_mingw

echo.
echo [ERROR] ビルドに使える C++ コンパイラが見つかりませんでした。
echo.
echo   Visual Studio 2022 を使う場合:
echo     Visual Studio Installer で
echo     「C++ によるデスクトップ開発」ワークロードをインストールしてください。
echo.
echo   MinGW-w64 / MSYS2 を使う場合:
echo     MSYS2 の UCRT64 シェルで次を実行してください。
echo       pacman -S mingw-w64-ucrt-x86_64-gcc make
echo.
echo   インストール後、この build.bat をもう一度実行してください。
exit /b 1

:do_msvc
call "%ROOT%build_msvc.bat"
exit /b %errorlevel%

:do_mingw
call "%ROOT%build_mingw.bat"
exit /b %errorlevel%

:do_test
call "%ROOT%build_test.bat"
exit /b %errorlevel%

:do_clean
call "%ROOT%clean.bat"
exit /b %errorlevel%

:usage
echo Migration Switcher - ビルド スクリプト
echo.
echo   build.bat            ツールチェーンを自動判定してビルド
echo   build.bat msvc       Visual Studio 2022 (cl.exe) でビルド
echo   build.bat mingw      MinGW-w64 / MSYS2 (g++) でビルド
echo   build.bat test       単体テストをビルドして実行
echo   build.bat clean      生成物を削除
echo   build.bat help       この使い方を表示
echo.
echo 生成物:
echo   build\Release\MigrationSwitcher.exe  (Visual Studio)
echo   build\MigrationSwitcher.exe          (MinGW)
exit /b 0

rem ---------------------------------------------------------------------------
rem  Visual Studio の検出（vcvars64.bat の実在で判定する）
rem ---------------------------------------------------------------------------
:detect_msvc
set "HAVE_MSVC=0"
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

if not defined VSPATH if defined VCINSTALLDIR set "VSPATH=%VCINSTALLDIR%.."
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
if not defined VSPATH exit /b 0

if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" exit /b 0
set "HAVE_MSVC=1"
exit /b 0

rem ---------------------------------------------------------------------------
rem  MinGW-w64 / MSYS2 の検出（実際に起動して判定する）
rem ---------------------------------------------------------------------------
:detect_mingw
set "HAVE_MINGW=0"
g++ --version >nul 2>nul
if not errorlevel 1 set "HAVE_MINGW=1"
if "%HAVE_MINGW%"=="1" exit /b 0
x86_64-w64-mingw32-g++ --version >nul 2>nul
if not errorlevel 1 set "HAVE_MINGW=1"
exit /b 0

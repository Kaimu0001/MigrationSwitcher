@echo off
rem ===========================================================================
rem  Migration Switcher - 単体テストのビルドと実行
rem
rem    build_test.bat            テストをビルドして実行
rem    build_test.bat live <ファイル>
rem                              実 API 疎通テストも実行（API キーを書いたファイル）
rem    build_test.bat clean      テスト生成物を削除
rem
rem  テストは GUI と同じソース（main.cpp を除く）をリンクしたコンソール版です。
rem ===========================================================================
setlocal
chcp 932 >nul
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "KEYFILE=%~2"
set "MODE=%~1"

echo ============================================================
echo  Migration Switcher - 単体テスト
echo ============================================================
echo  ルート: %ROOT%

if /i "%MODE%"=="clean" goto :clean

rem ---- ツールチェーンの判定 --------------------------------------------------
set "HAVE_MSVC=0"
set "HAVE_MINGW=0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH if defined VCINSTALLDIR set "VSPATH=%VCINSTALLDIR%.."
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
if not defined VSPATH if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
if defined VSPATH if exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" set "HAVE_MSVC=1"
if "%HAVE_MSVC%"=="0" (
  g++ --version >nul 2>nul
  if not errorlevel 1 set "HAVE_MINGW=1"
)
if "%HAVE_MSVC%"=="0" if "%HAVE_MINGW%"=="0" (
  x86_64-w64-mingw32-g++ --version >nul 2>nul
  if not errorlevel 1 set "HAVE_MINGW=1"
)

if "%HAVE_MSVC%"=="1" goto :msvc
if "%HAVE_MINGW%"=="1" goto :mingw
echo.
echo [ERROR] ビルドに使える C++ コンパイラが見つかりませんでした。
echo         build.bat help  を参照してください。
exit /b 1

rem ---- Visual Studio 2022 ----------------------------------------------------
:msvc
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if defined VSPATH if exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not defined VCToolsInstallDir goto :novs

set "TBUILD=%ROOT%\build\msvc_test"
set "TEXE=%TBUILD%\Release\ms_test.exe"
cmake --version >nul 2>nul
if errorlevel 1 goto :nocmake

echo  --- テストを構成しています (CMake) ---
cmake -S "%ROOT%\test" -B "%TBUILD%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail
echo  --- テストをビルドしています ---
cmake --build "%TBUILD%" --config Release
if errorlevel 1 goto :fail
goto :run

:nocmake
echo.
echo [ERROR] 単体テストのビルドには CMake が必要です。
echo         CMake をインストールするか、MinGW-w64 (build.bat test) を使用してください。
exit /b 1

rem ---- MinGW-w64 -------------------------------------------------------------
:mingw
set "MK="
mingw32-make --version >nul 2>nul
if not errorlevel 1 set "MK=mingw32-make"
if not defined MK (
  make --version >nul 2>nul
  if not errorlevel 1 set "MK=make"
)
if not defined MK goto :nomake
set "CROSSOPT="
g++ --version >nul 2>nul
if not errorlevel 1 set "CROSSOPT=CROSS="
cd /d "%ROOT%"
echo  --- テストをビルドしています (%MK%) ---
%MK% %CROSSOPT% build\ms_test.exe
if errorlevel 1 goto :fail
set "TEXE=%ROOT%\build\ms_test.exe"
goto :run

rem ---- 実行 ------------------------------------------------------------------
:run
if not exist "%TEXE%" goto :noexe
echo.
echo  --- テストを実行しています ---
echo  実行ファイル: %TEXE%
echo.
if /i "%MODE%"=="live" goto :live
"%TEXE%"
set "RC=%errorlevel%"
goto :result

:live
if not defined KEYFILE goto :nokey
if not exist "%KEYFILE%" goto :nokey
"%TEXE%" --live "%KEYFILE%"
set "RC=%errorlevel%"
goto :result

:result
echo.
if "%RC%"=="0" (
  echo ============================================================
  echo  テスト完了（すべて PASS）
  echo ============================================================
) else (
  echo ============================================================
  echo  テスト失敗（FAIL があります）
  echo ============================================================
)
echo  終了コード: %RC%
exit /b %RC%

:clean
echo  テスト生成物を削除しています
if exist "%ROOT%\build\msvc_test" rmdir /s /q "%ROOT%\build\msvc_test"
if exist "%ROOT%\build\console_test.o" del /q "%ROOT%\build\console_test.o" >nul 2>nul
if exist "%ROOT%\build\ms_test.exe" del /q "%ROOT%\build\ms_test.exe" >nul 2>nul
echo  完了しました。
exit /b 0

:novs
echo.
echo [ERROR] Visual Studio の cl.exe が見つかりません。
exit /b 1

:nomake
echo.
echo [ERROR] make / mingw32-make が見つかりません。
exit /b 1

:nokey
echo.
echo [ERROR] --live に指定する API キーのファイルが見つかりません。
echo         使い方:  build_test.bat live C:\path\to\key.txt
echo         （ファイルには API キーを 1 行だけ書いてください）
exit /b 1

:noexe
echo.
echo [ERROR] テスト実行ファイルが見つかりません。
echo         期待した場所: %TEXE%
exit /b 1

:fail
echo.
echo [ERROR] テストのビルドに失敗しました。
exit /b 1

@echo off
rem ===========================================================================
rem  Migration Switcher - Visual Studio 2022 ビルド
rem
rem    build_msvc.bat            ビルド（Release / x64）
rem    build_msvc.bat clean      生成物を削除
rem
rem  CMake があれば CMake（Visual Studio 17 2022 ジェネレータ）で、
rem  無ければ vcvars64 + cl.exe で直接ビルドします。
rem ===========================================================================
setlocal enabledelayedexpansion
chcp 932 >nul
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "SRC=%ROOT%\src"
set "BUILD=%ROOT%\build\msvc"
set "EXE=%BUILD%\Release\MigrationSwitcher.exe"

echo ============================================================
echo  Migration Switcher - Visual Studio 2022 ビルド (x64 / Release)
echo ============================================================
echo  ルート: %ROOT%

if /i "%~1"=="clean" goto :clean

rem ---- 1) Visual Studio の検出 ------------------------------------------------
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH if defined VCINSTALLDIR set "VSPATH=%VCINSTALLDIR%.."
if not defined VSPATH goto :novs
echo  VS: %VSPATH%

rem ---- 2) ビルド環境の読み込み ------------------------------------------------
if exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not defined VCToolsInstallDir goto :nocl

rem ---- 3) CMake があれば CMake でビルド --------------------------------------
cmake --version >nul 2>nul
if errorlevel 1 goto :direct

echo  --- CMake で構成しています ---
cmake -S "%ROOT%" -B "%BUILD%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :cfgfail

echo  --- ビルドしています ---
cmake --build "%BUILD%" --config Release
if errorlevel 1 goto :buildfail
goto :done

rem ---- 4) CMake が無い場合は cl.exe で直接ビルド ------------------------------
:direct
echo  --- CMake が見つからないため cl.exe で直接ビルドします ---
set "OBJ=%BUILD%\obj"
if not exist "%OBJ%" mkdir "%OBJ%" >nul 2>nul
pushd "%OBJ%"
cl /nologo /c /std:c++17 /O2 /W3 /EHsc /MP /DUNICODE /D_UNICODE /source-charset:.932 /execution-charset:.932 /I"%SRC%" "%SRC%\*.cpp"
set "CLERR=!errorlevel!"
popd
if not "!CLERR!"=="0" goto :buildfail

rc /nologo /fo "%OBJ%\app.res" /i "%SRC%" /dUNICODE /d_UNICODE "%SRC%\app.rc"
if errorlevel 1 goto :buildfail

if not exist "%BUILD%\Release" mkdir "%BUILD%\Release" >nul 2>nul
link /nologo /OUT:"%EXE%" /SUBSYSTEM:WINDOWS /MANIFEST:NO "%OBJ%\*.obj" "%OBJ%\app.res" comctl32.lib comdlg32.lib shlwapi.lib ole32.lib shell32.lib gdi32.lib user32.lib advapi32.lib winhttp.lib
if errorlevel 1 goto :buildfail

rem ---- 5) 完了 ----------------------------------------------------------------
:done
if not exist "%EXE%" goto :noexe
echo.
echo ============================================================
echo  ビルド成功
echo ============================================================
for %%f in ("%EXE%") do echo  出力: %%f  (%%~zf バイト)
echo.
echo  実行するには:  run.bat
echo  テストするには: build.bat test
exit /b 0

:clean
echo  生成物を削除しています: %BUILD%
if exist "%BUILD%" rmdir /s /q "%BUILD%"
echo  完了しました。
exit /b 0

:novs
echo.
echo [ERROR] Visual Studio 2022 が見つかりません。
echo         Visual Studio Installer で「C++ によるデスクトップ開発」を
echo         インストールしてください。
echo         または MinGW-w64 を使う場合は  build.bat mingw  を実行してください。
exit /b 1

:nocl
echo.
echo [ERROR] cl.exe が見つかりません。
echo         vcvars64.bat の読み込みに失敗した可能性があります。
echo         VS: %VSPATH%
exit /b 1

:cfgfail
echo.
echo [ERROR] CMake の構成に失敗しました。
echo         ログ: %BUILD%\CMakeFiles\CMakeError.log
exit /b 1

:buildfail
echo.
echo [ERROR] ビルドに失敗しました。
echo         上のコンパイラ エラーを確認してください。
exit /b 1

:noexe
echo.
echo [ERROR] ビルドは完了しましたが実行ファイルが見つかりません。
echo         期待した場所: %EXE%
exit /b 1

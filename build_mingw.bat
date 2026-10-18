@echo off
rem ===========================================================================
rem  Migration Switcher - MinGW-w64 / MSYS2 ビルド
rem
rem    build_mingw.bat            ビルド（Makefile を使用）
rem    build_mingw.bat clean      生成物を削除
rem
rem  MSYS2 の UCRT64 シェル相当の環境で実行してください。
rem    pacman -S mingw-w64-ucrt-x86_64-gcc make
rem ===========================================================================
setlocal
chcp 932 >nul
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "EXE=%ROOT%\build\MigrationSwitcher.exe"

echo ============================================================
echo  Migration Switcher - MinGW-w64 / MSYS2 ビルド
echo ============================================================
echo  ルート: %ROOT%

rem ---- make コマンドの検出 ----------------------------------------------------
set "MK="
mingw32-make --version >nul 2>nul
if not errorlevel 1 set "MK=mingw32-make"
if not defined MK (
  make --version >nul 2>nul
  if not errorlevel 1 set "MK=make"
)
if not defined MK goto :nomake
echo  make: %MK%

rem ---- コンパイラの検出（ネイティブなら CROSS を空にする）--------------------
set "CROSSOPT="
g++ --version >nul 2>nul
if not errorlevel 1 set "CROSSOPT=CROSS="
if defined CROSSOPT goto :go
x86_64-w64-mingw32-g++ --version >nul 2>nul
if errorlevel 1 goto :nocxx
echo  コンパイラ: x86_64-w64-mingw32-g++
goto :go

:go
cd /d "%ROOT%"

if /i "%~1"=="clean" (
  echo  --- 生成物を削除しています ---
  %MK% %CROSSOPT% clean
  echo  完了しました。
  exit /b 0
)

echo  --- ビルドしています ---
%MK% %CROSSOPT% all
if errorlevel 1 goto :buildfail

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

:nomake
echo.
echo [ERROR] make / mingw32-make が見つかりません。
echo         MSYS2 の UCRT64 シェルで次を実行してください。
echo           pacman -S mingw-w64-ucrt-x86_64-gcc make
exit /b 1

:nocxx
echo.
echo [ERROR] g++ が見つかりません。
echo         MSYS2 の UCRT64 シェルで次を実行してください。
echo           pacman -S mingw-w64-ucrt-x86_64-gcc make
echo         （MinGW-w64 を PATH に追加してから再実行してください）
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

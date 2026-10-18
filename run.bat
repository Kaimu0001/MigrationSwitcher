@echo off
rem ===========================================================================
rem  Migration Switcher - 実行
rem
rem    run.bat       ビルド済みの実行ファイルを起動します
rem
rem  実行ファイルが見つからない場合は build.bat を先に実行してください。
rem ===========================================================================
setlocal
chcp 932 >nul
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "EXE="
if exist "%ROOT%\build\Release\MigrationSwitcher.exe" set "EXE=%ROOT%\build\Release\MigrationSwitcher.exe"
if not defined EXE if exist "%ROOT%\build\MigrationSwitcher.exe" set "EXE=%ROOT%\build\MigrationSwitcher.exe"

if not defined EXE (
  echo.
  echo [ERROR] 実行ファイルが見つかりません。
  echo         先に  build.bat  を実行してください。
  echo         確認した場所:
  echo           %ROOT%\build\Release\MigrationSwitcher.exe
  echo           %ROOT%\build\MigrationSwitcher.exe
  exit /b 1
)

echo  起動: %EXE%
start "" "%EXE%"
exit /b 0

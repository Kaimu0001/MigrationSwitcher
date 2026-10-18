@echo off
rem ===========================================================================
rem  Migration Switcher - 生成物の削除
rem
rem    clean.bat          build フォルダを削除（ビルド生成物とテスト生成物）
rem    clean.bat all      config.ini / reports / backups / *.log も削除
rem
rem  ソース (src\ / test\ / docs\) は削除しません。
rem ===========================================================================
setlocal
chcp 932 >nul
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

echo ============================================================
echo  Migration Switcher - 生成物の削除
echo ============================================================
echo  ルート: %ROOT%

if exist "%ROOT%\build" (
  echo  - build\ を削除しています
  rmdir /s /q "%ROOT%\build"
) else (
  echo  - build\ は存在しません
)

if /i "%~1"=="all" (
  if exist "%ROOT%\config.ini" (
    echo  - config.ini を削除しています（API キーが含まれます）
    del /q "%ROOT%\config.ini" >nul 2>nul
  )
  if exist "%ROOT%\reports" (
    echo  - reports\ を削除しています
    rmdir /s /q "%ROOT%\reports"
  )
  if exist "%ROOT%\backups" (
    echo  - backups\ を削除しています
    rmdir /s /q "%ROOT%\backups"
  )
  if exist "%ROOT%\*.log" del /q "%ROOT%\*.log" >nul 2>nul
  if exist "%ROOT%\*.bak" del /q "%ROOT%\*.bak" >nul 2>nul
  if exist "%ROOT%\*.undobak" del /q "%ROOT%\*.undobak" >nul 2>nul
) else (
  echo.
  echo  config.ini / reports\ / backups\ も削除する場合は  clean.bat all
)

echo.
echo  完了しました。
exit /b 0

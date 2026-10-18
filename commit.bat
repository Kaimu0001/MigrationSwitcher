@echo off
chcp 932 > nul
cd /d "%~dp0"

echo ============================================================
echo  MigrationSwitcher : コミットと GitHub への push
echo ============================================================
echo.

git rev-parse --is-inside-work-tree > nul 2>&1
if errorlevel 1 (
    echo [中止] このフォルダは git リポジトリではありません。
    echo        D:\C++\MigrationSwitcher に置いて実行してください。
    pause
    exit /b 1
)

rem --- API キー平文の config.ini が追跡されていたら中止する ---
git ls-files config.ini | findstr /i "config.ini" > nul
if not errorlevel 1 (
    echo [中止] config.ini が git の追跡対象になっています。
    echo        API キーが平文で入っているため push しません。
    echo        次の 2 行を実行してから、もう一度このファイルを実行してください。
    echo.
    echo            git rm --cached config.ini
    echo            git commit -m "config.ini を追跡対象から除外"
    echo.
    pause
    exit /b 1
)

set "MSG=%TEMP%\ms_commit_msg.txt"

>  "%MSG%" echo EUC-JP 判定の OS 依存を解消し、UI と挙動の不具合 4 件を修正
>> "%MSG%" echo.
>> "%MSG%" echo - EUC-JP: kernel32 にコードページ 51932 が無く判定が常に失敗していた問題を修正。
>> "%MSG%" echo   JIS X 0208 / JIS X 0212 の内蔵テーブル src/EucJpTable.h と .cpp で判定と変換を行うよう変更。
>> "%MSG%" echo   自動判定は CP932 で半角カタカナが現れ、全角の日本語文字が EUC-JP 読みより少ない場合に EUC-JP とする。
>> "%MSG%" echo - 警告: include guard を既存コンパイルスイッチ警告から除外し、内側判定を優先。
>> "%MSG%" echo - 警告オプションの既定を有効に修正し、チェック時に即保存するよう変更。
>> "%MSG%" echo - 基本設定タブ: AI 連携の接続テストボタンがグループ枠の外に出ていたのを修正。
>> "%MSG%" echo - タブ順を 検索条件 旧コード 新コード の順に変更。PG_SEARCH=2 / PG_OLD=3。
>> "%MSG%" echo - 置換後も検索結果を保持。クリアは再走査と再検索を実行したときだけ。
>> "%MSG%" echo   あわせて置換後の内容を読み直し、次回実行時の検索後に変更されています の誤判定を防止。
>> "%MSG%" echo - WinMerge: 選択した全ファイルをファイル単位で比較するよう変更。1 ファイル 1 ウィンドウ、上限 10。
>> "%MSG%" echo - MSVC で不足していた標準ヘッダ iterator cstdio cwchar を追加。
>> "%MSG%" echo - docs: 操作手順書に警告判定ルール 16.9 と EUC-JP の扱いを追記。
>> "%MSG%" echo.
>> "%MSG%" echo テスト: 428 PASS / 0 FAIL

echo --- ステージング ---
git add -A
git status --short
echo.

echo --- コミット ---
git commit -F "%MSG%"
if errorlevel 1 (
    echo.
    echo [情報] コミットできませんでした。変更が無いか、上記のメッセージを確認してください。
    del "%MSG%" > nul 2>&1
    pause
    exit /b 1
)
del "%MSG%" > nul 2>&1
echo.

echo --- push ---
git push origin main
echo.

echo --- 結果 ---
git log --oneline -3
echo.
git status
echo.
echo 完了しました。上の status が working tree clean なら push 済みです。
pause

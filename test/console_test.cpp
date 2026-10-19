// =============================================================================
// このファイルはコンソール版テストハーネスです（GUI 版とは別の実行ファイル）。
// GUI プロジェクトへ誤って追加されても main が重複しないよう、
// MS_BUILD_CONSOLE_TESTS が定義されているときだけ中身をコンパイルします。
//   Makefile : make build/ms_test.exe   (-DMS_BUILD_CONSOLE_TESTS を付与)
//   CMake    : test/CMakeLists.txt の ms_test ターゲット
// =============================================================================
#ifdef MS_BUILD_CONSOLE_TESTS
// =============================================================================
// console_test.cpp
// 設計書 20.1「単体テスト」に対応するコンソール検証ハーネス
// GUI と同じオブジェクトファイル（main.cpp を除く）をリンクして実行する。
//   make test
// =============================================================================
#include <windows.h>
#include <cwchar>
#include <cstdio>
#include <string>
#include <vector>

#include "Models.h"
#include "Utils.h"
#include "LogRuntimeGenerator.h"
#include "CodeLogger.h"
#include "ScopeReportGenerator.h"
#include "GTestGenerator.h"
#include "EncodingDetector.h"
#include "FileManager.h"
#include "SearchEngine.h"
#include "CodeScope.h"
#include "ResultExporter.h"
#include "SwitchGenerator.h"
#include "PreviewGenerator.h"
#include "DiffReportGenerator.h"
#include "BackupManager.h"
#include "UndoManager.h"
#include "ValidationManager.h"
#include "FolderScanner.h"
#include "BatchProcessor.h"
#include "SwitchHeaderManager.h"
#include "AiClient.h"
#include "ConfigManager.h"
#include "SpecGenerator.h"

using namespace ms;

static int g_pass = 0;
static int g_fail = 0;

static void Out(const std::wstring& w)
{
    std::string u = util::WideToUtf8(w);
    fputs(u.c_str(), stdout);
}

static void Check(bool cond, const std::wstring& name)
{
    if (cond) { ++g_pass; Out(L"  [PASS] " + name + L"\n"); }
    else      { ++g_fail; Out(L"  [FAIL] " + name + L"\n"); }
}

static bool Contains(const std::wstring& hay, const std::wstring& needle)
{
    return hay.find(needle) != std::wstring::npos;
}

static void CleanDir(const std::wstring& dir)
{
    std::wstring pattern = util::JoinPath(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
            continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        FileManager::RemoveFile(util::JoinPath(dir, fd.cFileName));
    } while (::FindNextFileW(h, &fd));
    ::FindClose(h);
}

static std::wstring TestDir()
{
    wchar_t tmp[MAX_PATH];
    DWORD n = ::GetTempPathW(MAX_PATH, tmp);
    std::wstring base = (n > 0) ? std::wstring(tmp) : util::ModuleDirectory();
    std::wstring dir = util::JoinPath(base, L"ms_test");
    util::EnsureDirectory(dir);
    return dir;
}

static bool WriteRaw(const std::wstring& path, const std::string& bytes)
{
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    if (!bytes.empty()) ::WriteFile(h, bytes.data(), (DWORD)bytes.size(), &w, nullptr);
    ::CloseHandle(h);
    return w == (DWORD)bytes.size();
}

static bool ReadRaw(const std::wstring& path, std::string& out)
{
    return FileManager::ReadAllBytes(path, out);
}

// -----------------------------------------------------------------------------
// 20.1 No.1～2 スイッチ名検証
// -----------------------------------------------------------------------------
static void TestSwitchName()
{
    Out(L"\n== スイッチ名検証 (20.1 No.1-2) ==\n");

    Check(ValidationManager::ValidateSwitchName(L"MIGRATION_001").ok,
          L"正常名 MIGRATION_001 を許可する");
    Check(ValidationManager::ValidateSwitchName(L"USE_NEW_LOGIC").ok,
          L"正常名 USE_NEW_LOGIC を許可する");
    Check(ValidationManager::ValidateSwitchName(L"ENABLE_NEW_API").ok,
          L"正常名 ENABLE_NEW_API を許可する");
    Check(ValidationManager::ValidateSwitchName(L"_PRIVATE").ok,
          L"先頭アンダースコアを許可する");
    Check(!ValidationManager::ValidateSwitchName(L"123_SWITCH").ok,
          L"数字始まりを拒否する");
    Check(!ValidationManager::ValidateSwitchName(L"SWITCH-NAME").ok,
          L"ハイフンを拒否する");
    Check(!ValidationManager::ValidateSwitchName(L"SWITCH NAME").ok,
          L"空白を拒否する");
    Check(!ValidationManager::ValidateSwitchName(L"").ok,
          L"未入力を拒否する");
    Check(!ValidationManager::ValidateSwitchName(L"SWITCH.NAME").ok,
          L"記号を拒否する");
    Check(!ValidationManager::ValidateSwitchName(L"MIGRATION_001;").ok,
          L"末尾のセミコロンを拒否する");

    ValidationResult r = ValidationManager::ValidateSwitchName(L"if");
    Check(r.ok && !r.warnings.empty(), L"予約語 if を警告する");
}

// -----------------------------------------------------------------------------
// 20.1 No.3～5 検索
// -----------------------------------------------------------------------------
static void TestSearch()
{
    Out(L"\n== 検索 (20.1 No.3-5) ==\n");

    std::wstring text =
        L"int a;\n"
        L"old_function(name, g_counter);\n"
        L"int b;\n"
        L"    old_function(name, g_counter);\n"
        L"int c;\n"
        L"/* old_function(name, g_counter); */\n";

    std::wstring pat = L"old_function(name, g_counter);";

    std::vector<MatchResult> m = SearchEngine::Search(text, pat, L"sample.c");
    Check(m.size() == 3, L"3 件ヒットする（複数件ヒット）");
    if (m.size() == 3) {
        Check(m[0].start_line == 2 && m[0].start_column == 1, L"1 件目の行番号=2, 列番号=1");
        Check(m[1].start_line == 4 && m[1].start_column == 5, L"2 件目の行番号=4, 列番号=5");
        Check(m[2].start_line == 6, L"3 件目の行番号=6（コメント内）");
    }

    std::vector<MatchResult> none = SearchEngine::Search(text, L"not_exist_symbol", L"sample.c");
    Check(none.empty(), L"0 件の場合を正しく処理する");

    std::vector<MatchResult> one = SearchEngine::Search(L"only once\n", L"once", L"x.c");
    Check(one.size() == 1, L"1 件ヒットする");

    // 15章 既存コンパイルスイッチ検出
    std::wstring sw_text =
        L"#ifdef LEGACY\n"
        L"#endif\n"
        L"int x;\n"
        L"old_function(a, b);\n"
        L"int y;\n";
    std::wstring detail;
    bool nearby = SearchEngine::DetectNearbyPreprocessor(sw_text, 4, 4, 5, detail);
    Check(nearby, L"既存 #ifdef を近傍検出する");
    Check(!SearchEngine::DetectNearbyPreprocessor(L"int a;\nint b;\n", 1, 1, 5, detail),
          L"プリプロセッサが無い場合は検出しない");

    // 16章 コメント内ヒット
    Check(SearchEngine::IsInsideComment(text, m[2].start_offset),
          L"/* */ 内の一致をコメント内と判定する");
    Check(!SearchEngine::IsInsideComment(text, m[0].start_offset),
          L"通常行の一致はコメント内と判定しない");

    std::wstring str_text = L"const char* s = \"old_function(name, g_counter);\";\n";
    std::vector<MatchResult> sm = SearchEngine::Search(str_text, pat, L"s.c");
    Check(sm.size() == 1 && !SearchEngine::IsInsideComment(str_text, sm[0].start_offset),
          L"文字列リテラル内はコメント扱いしない");
}

// -----------------------------------------------------------------------------
// 20.1 No.6～7 変換（6.7 変換方式A～D）
// -----------------------------------------------------------------------------
static void TestGenerate()
{
    Out(L"\n== コンパイルスイッチ生成 (20.1 No.6-7) ==\n");

    std::wstring sw = L"MIGRATION_001";
    std::wstring oldc = L"old_function(param1, param2);\nreturn result;";
    std::wstring newc = L"new_function(param1, param2);\nreturn new_result;";
    std::vector<std::wstring> w;

    std::wstring a = SwitchGenerator::Generate(SwitchStyle::IfdefNewFirst, sw, oldc, newc, false, L"", w);
    std::wstring expected_a =
        L"#ifdef MIGRATION_001\n"
        L"new_function(param1, param2);\n"
        L"return new_result;\n"
        L"#else\n"
        L"old_function(param1, param2);\n"
        L"return result;\n"
        L"#endif";
    Check(a == expected_a, L"方式A(#ifdef 新コード優先) が設計書 6.7.2 の出力と一致する");

    std::wstring b = SwitchGenerator::Generate(SwitchStyle::IfndefOldFirst, sw, oldc, newc, false, L"", w);
    Check(Contains(b, L"#ifndef MIGRATION_001\nold_function(param1, param2);\nreturn result;\n#else\nnew_function(param1, param2);\nreturn new_result;\n#endif"),
          L"方式B(#ifndef 旧コード優先) が設計書 6.7.3 の出力と一致する");

    std::wstring c = SwitchGenerator::Generate(SwitchStyle::IfdefWithComments, sw, oldc, newc, false, L"", w);
    Check(Contains(c, L"#ifdef MIGRATION_001  /* migration start */") &&
          Contains(c, L"#else               /* original */") &&
          Contains(c, L"#endif              /* MIGRATION_001 */"),
          L"方式C(コメント付き) が設計書 6.7.4 の出力と一致する");

    std::wstring d = SwitchGenerator::Generate(SwitchStyle::BlockCommentStyle, sw, oldc, newc, false, L"", w);
    Check(Contains(d, L"/* MIGRATION_001 start */") &&
          Contains(d, L"/* MIGRATION_001 end */") &&
          Contains(d, L"#ifdef MIGRATION_001"),
          L"方式D(開始終了コメント) が設計書 6.7.5 の出力と一致する");

    // 18章 テンプレート方式
    std::wstring tpl = L"#ifdef ${SWITCH}\n${NEW_CODE}\n#else\n${OLD_CODE}\n#endif";
    std::wstring t = SwitchGenerator::Generate(SwitchStyle::CustomTemplate, sw, oldc, newc, false, tpl, w);
    Check(t == expected_a, L"テンプレート方式が方式Aと同一の出力を生成する");

    std::wstring bad_err;
    Check(!SwitchGenerator::ValidateTemplate(L"#ifdef X\n#endif", bad_err),
          L"不正テンプレート（変数なし）を検出する");
}

// -----------------------------------------------------------------------------
// 6.8 インデント自動調整
// -----------------------------------------------------------------------------
static void TestAutoIndent()
{
    Out(L"\n== インデント自動調整 (6.8) ==\n");

    std::wstring oldc = L"        old_function();";
    std::wstring newc = L"new_function();";
    std::vector<std::wstring> w;
    std::wstring out = SwitchGenerator::Generate(
        SwitchStyle::IfdefNewFirst, L"MIGRATION_001", oldc, newc, true, L"", w);

    std::wstring expected =
        L"#ifdef MIGRATION_001\n"
        L"        new_function();\n"
        L"#else\n"
        L"        old_function();\n"
        L"#endif";
    Check(out == expected, L"設計書 6.8.2 の出力例と一致する");

    // 6.8.3 新コードに既にインデントがある場合は調整しない
    std::vector<std::wstring> w2;
    std::wstring out2 = SwitchGenerator::Generate(
        SwitchStyle::IfdefNewFirst, L"SW", L"    old();", L"    new();", true, L"", w2);
    Check(!w2.empty(), L"新コードに既存インデントがある場合に警告する");
    Check(Contains(out2, L"    new();"), L"既存インデントを二重に付与しない");

    // 6.8.3 タブとスペース混在
    std::vector<std::wstring> w3;
    SwitchGenerator::ApplyAutoIndent(L" \told();", L"new();", w3);
    Check(!w3.empty(), L"タブとスペース混在時に警告する");
}

// -----------------------------------------------------------------------------
// 9.3 / 12章 プレビューと置換
// -----------------------------------------------------------------------------
static void TestPreviewAndReplace()
{
    Out(L"\n== プレビュー生成と置換 (9.3 / 12章) ==\n");

    std::wstring text =
        L"void f1() {\n"
        L"    old_function(a, b);\n"
        L"}\n"
        L"void f2() {\n"
        L"    old_function(a, b);\n"
        L"}\n";
    std::wstring pat = L"old_function(a, b);";
    std::vector<MatchResult> m = SearchEngine::Search(text, pat, L"x.c");
    Check(m.size() == 2, L"2 件ヒットする");

    std::wstring block =
        L"#ifdef MIGRATION_001\nnew_function(a, b);\n#else\nold_function(a, b);\n#endif";

    // 1 件目のみ選択
    m[0].selected = true;
    std::wstring after1 = PreviewGenerator::BuildAfterText(text, m, block);
    Check(after1.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
          L"選択箇所にスイッチが挿入される");
    // 置換されなかった 2 件目が残っていること
    size_t first = after1.find(L"old_function(a, b);");
    size_t second = after1.find(L"old_function(a, b);", first + 1);
    Check(second != std::wstring::npos, L"未選択の 2 件目はそのまま残る");

    // 全件選択
    m[0].selected = true;
    m[1].selected = true;
    std::wstring after2 = PreviewGenerator::BuildAfterText(text, m, block);
    int cnt = 0;
    size_t pos = 0;
    while ((pos = after2.find(L"#ifdef MIGRATION_001", pos)) != std::wstring::npos) { ++cnt; pos += 4; }
    Check(cnt == 2, L"2 件すべてにスイッチが挿入される");

    // 抜粋生成
    std::wstring bx, ax;
    bool ok = PreviewGenerator::BuildExcerpts(text, after2, m, 2, bx, ax);
    Check(ok && !bx.empty() && !ax.empty(), L"変更前後の抜粋を生成できる");

    // 表示モード
    Check(Contains(PreviewGenerator::BuildTopBottom(bx, ax), L"===== 変更前 ====="),
          L"前後比較表示のヘッダを生成する");
    Check(Contains(PreviewGenerator::BuildSideBySide(bx, ax), L"| ===== 変更後"),
          L"左右比較表示のヘッダを生成する");
    Check(Contains(PreviewGenerator::BuildSimple(ax), L"===== 変更後 ====="),
          L"単純表示のヘッダを生成する");
}

// -----------------------------------------------------------------------------
// 3.4 / 6.5.3 文字コード・改行コード
// -----------------------------------------------------------------------------
static void TestEncodingAndNewline()
{
    Out(L"\n== 文字コード / 改行コード (3.4 / 6.5.3) ==\n");

    std::wstring dir = TestDir();

    // --- UTF-8 (BOM なし) + CRLF ---
    {
        std::wstring path = util::JoinPath(dir, L"utf8_crlf.c");
        std::wstring text = L"int a;\r\nold_function(x);\r\n";
        std::string bytes;
        EncodingDetector::Encode(text, TextEncoding::UTF8, false, bytes);
        Check(WriteRaw(path, bytes), L"UTF-8/CRLF のテストファイルを作成");

        std::wstring read;
        FileInfo fi;
        Check(FileManager::ReadText(path, L"auto", read, fi), L"UTF-8/CRLF を読み込める");
        Check(fi.encoding == TextEncoding::UTF8, L"UTF-8 と判定する");
        Check(fi.newline == NewlineKind::CRLF, L"CRLF と判定する");
        Check(read == L"int a;\nold_function(x);\n", L"内部表現で LF に正規化される");

        std::wstring after = L"int a;\n#ifdef SW\nnew_function(x);\n#else\nold_function(x);\n#endif\n";
        std::wstring err;
        Check(FileManager::WriteTextAtomic(path, after, fi.encoding, fi.has_bom, fi.newline, err),
              L"CRLF を維持して書き戻せる");
        std::string raw2;
        ReadRaw(path, raw2);
        Check(raw2.find("\r\n") != std::string::npos, L"書き戻し後も CRLF が維持される");
    }

    // --- UTF-8 BOM 付き + LF ---
    {
        std::wstring path = util::JoinPath(dir, L"utf8bom_lf.c");
        std::string bytes;
        EncodingDetector::Encode(L"int b;\nold_function(y);\n", TextEncoding::UTF8, true, bytes);
        WriteRaw(path, bytes);

        std::wstring read;
        FileInfo fi;
        FileManager::ReadText(path, L"auto", read, fi);
        Check(fi.encoding == TextEncoding::UTF8BOM, L"UTF-8 BOM 付きと判定する");
        Check(fi.newline == NewlineKind::LF, L"LF と判定する");
        Check(read == L"int b;\nold_function(y);\n", L"BOM を除去して検索対象化する");

        std::wstring err;
        FileManager::WriteTextAtomic(path, L"int b;\nold_function(y);\n",
                                     fi.encoding, fi.has_bom, fi.newline, err);
        std::string raw2;
        ReadRaw(path, raw2);
        Check(raw2.size() >= 3 && (unsigned char)raw2[0] == 0xEF &&
              (unsigned char)raw2[1] == 0xBB && (unsigned char)raw2[2] == 0xBF,
              L"書き戻し後も BOM が維持される");
        Check(raw2.find("\r\n") == std::string::npos, L"書き戻し後も LF が維持される");
    }

    // --- Shift_JIS ---
    {
        std::wstring path = util::JoinPath(dir, L"sjis.c");
        std::wstring text = L"/* 日本語コメント */\r\nold_function(z);\r\n";
        std::string bytes;
        EncodingDetector::Encode(text, TextEncoding::ShiftJIS, false, bytes);
        WriteRaw(path, bytes);

        std::wstring read;
        FileInfo fi;
        FileManager::ReadText(path, L"auto", read, fi);
        Check(fi.encoding == TextEncoding::ShiftJIS, L"Shift_JIS と判定する");
        Check(read == L"/* 日本語コメント */\nold_function(z);\n",
              L"Shift_JIS の日本語が文字化けせず読み込める");

        std::wstring err;
        Check(FileManager::WriteTextAtomic(path, read, fi.encoding, fi.has_bom, fi.newline, err),
              L"Shift_JIS で書き戻せる");
        std::string raw2;
        ReadRaw(path, raw2);
        Check(raw2 == bytes, L"Shift_JIS のバイト列が完全に一致する（往復一致）");
    }

    // --- 判定関数単体 ---
    {
        size_t bom = 0;
        std::string utf8 = "\xE3\x81\x82";     // "あ"
        Check(EncodingDetector::Detect(utf8, bom) == TextEncoding::UTF8, L"UTF-8 を判定する");
        std::string sjis = "\x82\xA0";         // "あ" (Shift_JIS)
        Check(EncodingDetector::Detect(sjis, bom) == TextEncoding::ShiftJIS, L"Shift_JIS を判定する");
        std::string bombytes = "\xEF\xBB\xBFx";
        size_t bl = 0;
        Check(EncodingDetector::Detect(bombytes, bl) == TextEncoding::UTF8BOM && bl == 3,
              L"BOM を検出し長さ 3 を返す");
    }
}

// -----------------------------------------------------------------------------
// 20.1 No.8～9 バックアップ / Undo
// -----------------------------------------------------------------------------
static void TestBackupUndo()
{
    Out(L"\n== バックアップ / Undo (20.1 No.8-9) ==\n");

    std::wstring dir = TestDir();
    std::wstring target = util::JoinPath(dir, L"backup_target.c");
    std::wstring original = L"int original_content;\n";
    std::wstring err;

    std::string bytes;
    EncodingDetector::Encode(original, TextEncoding::UTF8, false, bytes);
    WriteRaw(target, bytes);

    std::wstring backup;
    Check(BackupManager::CreateBackup(target, L"", backup, err),
          L"バックアップを作成する（同一フォルダ）");
    Check(!backup.empty() && util::PathExists(backup), L"バックアップファイルが実在する");
    Check(backup.find(L".bak") != std::wstring::npos, L"拡張子が .bak である");
    Check(util::GetDirectoryOf(backup) == util::GetDirectoryOf(target),
          L"バックアップ先が対象と同一フォルダである");

    std::wstring cberr;
    Check(BackupManager::CanCreateBackup(target, L"", cberr),
          L"バックアップ作成可否を事前確認できる");

    // 変更を加える
    std::wstring err2;
    FileManager::WriteTextAtomic(target, L"int changed_content;\n",
                                 TextEncoding::UTF8, false, NewlineKind::LF, err2);

    std::wstring read;
    FileInfo fi;
    FileManager::ReadText(target, L"auto", read, fi);
    Check(read == L"int changed_content;\n", L"変更後の内容が書き込まれている");

    // Undo
    UndoManager um;
    UndoRecord rec;
    rec.target_file_path = target;
    rec.backup_file_path = backup;
    rec.switch_name = L"MIGRATION_001";
    rec.executed_at = util::NowDateTime();
    um.SetRecord(rec);
    Check(um.CanUndo(), L"Undo 可能と判定する");

    std::wstring uerr;
    std::vector<std::wstring> sidecar;
    Check(um.Undo(uerr, sidecar), L"バックアップから復元できる");
    FileManager::ReadText(target, L"auto", read, fi);
    Check(read == original, L"元ファイルの内容へ完全に復元される");
    Check(!um.CanUndo(), L"初期版では Undo は 1 回のみ（2 回目は不可）");
}

// -----------------------------------------------------------------------------
// 20.1 No.10 HTML レポート生成
// -----------------------------------------------------------------------------
static void TestReport()
{
    Out(L"\n== HTML 差分レポート (20.1 No.10 / 6.12) ==\n");

    std::wstring dir = TestDir();
    std::wstring target = util::JoinPath(dir, L"report_target.c");

    std::wstring before =
        L"void f() {\n"
        L"    old_function(a, b);\n"
        L"    return;\n"
        L"}\n";
    std::vector<MatchResult> m = SearchEngine::Search(before, L"old_function(a, b);", target);
    m[0].selected = true;
    m[0].warning_flags.push_back(L"テスト警告");

    std::wstring block =
        L"#ifdef MIGRATION_001\nnew_function(a, b);\n#else\nold_function(a, b);\n#endif";
    std::wstring after = PreviewGenerator::BuildAfterText(before, m, block);

    OperationContext ctx;
    ctx.target_file_path = target;
    ctx.switch_name = L"MIGRATION_001";
    ctx.old_code = L"old_function(a, b);";
    ctx.new_code = L"new_function(a, b);";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.execution_mode = ExecutionMode::DryRun;
    ctx.encoding = TextEncoding::UTF8;
    ctx.newline = NewlineKind::CRLF;
    ctx.started_at = util::NowDateTime();
    ctx.selected_matches = m;

    ReplaceResult res;
    res.replaced_count = 1;
    res.warning_count = 1;
    res.status = L"Warning";
    res.executed_at = util::NowDateTime();

    std::wstring path = DiffReportGenerator::BuildReportPath(dir, target, util::TimestampFileName());
    Check(path.find(L"_report_target_diff.html") != std::wstring::npos,
          L"レポートファイル名が <stamp>_<basename>_diff.html 形式である");

    std::wstring err;
    Check(DiffReportGenerator::Generate(path, ctx, m, before, after, res, err),
          L"HTML レポートを生成する");
    Check(util::PathExists(path), L"レポートファイルが実在する");

    std::string raw;
    ReadRaw(path, raw);
    std::wstring html = util::Utf8ToWide(
        raw.size() > 3 ? raw.substr(3) : raw);   // BOM を除去
    Check(Contains(html, L"Migration Switch Report"), L"タイトルが含まれる");
    Check(Contains(html, L"MIGRATION_001"), L"スイッチ名が含まれる");
    Check(Contains(html, L"ドライラン"), L"実行モード（ドライラン）が含まれる");
    Check(Contains(html, L".added{background-color:#e6ffed;}"),
          L"CSS 方針 .added = #e6ffed が含まれる");
    Check(Contains(html, L".deleted{background-color:#ffeef0;}"),
          L"CSS 方針 .deleted = #ffeef0 が含まれる");
    Check(Contains(html, L".changed{background-color:#fff5b1;}"),
          L"CSS 方針 .changed = #fff5b1 が含まれる");
    Check(Contains(html, L".warning{background-color:#ffd580;}"),
          L"CSS 方針 .warning = #ffd580 が含まれる");
    Check(Contains(html, L"old_function(a, b);") && Contains(html, L"new_function(a, b);"),
          L"変更前後のコードが含まれる");
    Check(Contains(html, L"テスト警告"), L"警告一覧が含まれる");
}

// -----------------------------------------------------------------------------
// 19章 安全性（読み取り専用・原子的書き込み）
// -----------------------------------------------------------------------------
static void TestSafety()
{
    Out(L"\n== 安全性 (19章) ==\n");

    std::wstring dir = TestDir();
    std::wstring ro = util::JoinPath(dir, L"readonly.c");
    std::string bytes;
    EncodingDetector::Encode(L"int a;\n", TextEncoding::UTF8, false, bytes);
    WriteRaw(ro, bytes);
    ::SetFileAttributesW(ro.c_str(), FILE_ATTRIBUTE_READONLY);

    FileInfo fi = FileManager::Inspect(ro, L"auto");
    Check(fi.read_only_attribute, L"読み取り専用属性を検出する");
    Check(!fi.writable, L"読み取り専用ファイルは書き込みハンドルを取得できない（Windows の仕様どおり）");
    {
        ValidationResult rovr = ValidationManager::ValidateTargetFile(ro, fi);
        Check(rovr.ok && !rovr.warnings.empty(),
              L"読み取り専用でも警告のみで実行を中断しない（属性を一時解除して書き込む設計）");
    }

    std::wstring err;
    bool wrote = FileManager::WriteTextAtomic(ro, L"int changed;\n",
                                              TextEncoding::UTF8, false,
                                              NewlineKind::LF, err);
    Check(wrote, L"読み取り専用ファイルへ書き込める（属性を一時解除）");

    DWORD attr = ::GetFileAttributesW(ro.c_str());
    Check((attr & FILE_ATTRIBUTE_READONLY) != 0, L"書き込み後に読み取り専用属性が復元される");
    ::SetFileAttributesW(ro.c_str(), FILE_ATTRIBUTE_NORMAL);

    std::wstring missing = util::JoinPath(dir, L"no_such_file.c");
    FileInfo mi = FileManager::Inspect(missing, L"auto");
    Check(!mi.exists, L"存在しないファイルを検出する");
    ValidationResult vr = ValidationManager::ValidateTargetFile(missing, mi);
    Check(!vr.ok, L"存在しないファイルを実行前チェックで中断する");

    // 一時ファイル経由であること（temp フォルダが作られる）
    Check(util::PathExists(util::JoinPath(util::ModuleDirectory(), L"temp")),
          L"一時ファイル用 temp フォルダが作成される");
}

// -----------------------------------------------------------------------------
// 6.3 / 6.4 入力チェック
// -----------------------------------------------------------------------------
static void TestInputChecks()
{
    Out(L"\n== 入力チェック (6.3 / 6.4) ==\n");

    Check(!ValidationManager::ValidateOldCode(L"").ok, L"旧コード未入力をエラーにする");
    Check(!ValidationManager::ValidateOldCode(L"   \n\t ").ok, L"旧コード空白のみをエラーにする");
    Check(ValidationManager::ValidateOldCode(L"old_function(a);").ok, L"正常な旧コードを許可する");

    ValidationResult n1 = ValidationManager::ValidateNewCode(L"old();", L"");
    Check(n1.ok && !n1.warnings.empty(), L"新コード未入力を警告する");

    ValidationResult n2 = ValidationManager::ValidateNewCode(L"old();", L"old();");
    Check(!n2.warnings.empty(), L"旧コードと同一の新コードを警告する");

    ValidationResult n3 = ValidationManager::ValidateNewCode(L"old();", L"#ifdef X\nnew();\n#endif");
    bool has_ifdef_warn = false;
    for (size_t i = 0; i < n3.warnings.size(); ++i)
        if (n3.warnings[i].find(L"#ifdef") != std::wstring::npos) has_ifdef_warn = true;
    Check(has_ifdef_warn, L"新コードの #ifdef 混入を警告する");
}


// =============================================================================
// Phase 4: 17章 複数ファイル一括処理
// =============================================================================
static bool WriteTextFile(const std::wstring& path, const std::wstring& text)
{
    std::string bytes;
    EncodingDetector::Encode(text, TextEncoding::UTF8, false, bytes);
    return WriteRaw(path, bytes);
}

static std::wstring MakeBatchTree()
{
    std::wstring base = util::JoinPath(TestDir(), L"batch");
    util::EnsureDirectory(base);
    std::wstring sub = util::JoinPath(base, L"sub");
    util::EnsureDirectory(sub);

    WriteTextFile(util::JoinPath(base, L"a.c"),
        L"int a1(void)\n{\n    old_function();\n    return 0;\n}\n"
        L"int a2(void)\n{\n        old_function();\n        return 0;\n}\n");
    WriteTextFile(util::JoinPath(base, L"b.c"),
        L"void b1(void)\n{\n    old_function();\n}\n");
    WriteTextFile(util::JoinPath(sub, L"c.h"),
        L"static void c1(void)\n{\n    old_function();\n}\n");
    WriteTextFile(util::JoinPath(base, L"note.txt"),
        L"old_function();\n");
    return base;
}

static void BuildBlocks(std::vector<BatchFileEntry>& entries,
                        std::vector<MatchResult>& matches,
                        std::vector<std::wstring>& blocks)
{
    blocks.assign(matches.size(), std::wstring());
    for (size_t i = 0; i < matches.size(); ++i) {
        std::wstring text;
        for (size_t k = 0; k < entries.size(); ++k)
            if (entries[k].path == matches[i].file_path) { text = entries[k].text; break; }
        std::vector<std::wstring> w;
        std::wstring hint = SearchEngine::IndentHintAt(text, matches[i]);
        blocks[i] = SwitchGenerator::GenerateEx(
            SwitchStyle::IfdefNewFirst, L"MIGRATION_001",
            L"old_function();", L"new_function();", true, L"", hint, w);
    }
}

static void TestFolderScan()
{
    Out(L"\n== フォルダ走査 / 拡張子フィルタ (17.2.2 手順1-2) ==\n");

    std::wstring base = MakeBatchTree();
    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    exts.push_back(L".h");

    ScanResult all;
    FolderScanner::Scan(base, exts, true, all);
    Check(all.files.size() == 3, L"サブフォルダを含めて 3 ファイルを抽出する（.txt は除外）");

    ScanResult flat;
    FolderScanner::Scan(base, exts, false, flat);
    Check(flat.files.size() == 2, L"サブフォルダを含めない場合は 2 ファイルを抽出する");

    std::vector<std::wstring> cexts;
    cexts.push_back(L".c");
    ScanResult conly;
    FolderScanner::Scan(base, cexts, true, conly);
    Check(conly.files.size() == 2, L"拡張子フィルタ .c のみでは 2 ファイルを抽出する");

    Check(FolderScanner::MatchesExtension(L"C:\\work\\Y.C", exts),
          L"拡張子の大文字小文字を区別せず一致する");
    Check(!FolderScanner::MatchesExtension(L"C:\\work\\y.cpp", exts),
          L"フィルタ外の拡張子は一致しない");

    std::wstring joined = FolderScanner::JoinExtensions(exts);
    Check(joined == L"c,h", L"拡張子リストを config 用に結合できる");
    std::vector<std::wstring> back = FolderScanner::SplitExtensions(L"c, cpp ,h");
    Check(back.size() == 3 && back[0] == L".c" && back[1] == L".cpp" && back[2] == L".h",
          L"拡張子リストを復元できる（空白・ドット有無を吸収）");

    std::vector<std::wstring> defs = FolderScanner::DefaultExtensions();
    Check(defs.size() == 3, L"既定の拡張子フィルタは 3 種（17.2.1）");
    Check(FolderScanner::CandidateExtensions().size() == 6,
          L"拡張子候補は 6 種（.c .cpp .h .hpp .cs .java）");

    ScanResult missing;
    FolderScanner::Scan(util::JoinPath(TestDir(), L"no_such_dir"), exts, true, missing);
    Check(missing.files.empty() && !missing.errors.empty(),
          L"存在しないフォルダを検出する");
}

static void TestBatchSearch()
{
    Out(L"\n== 一括検索と集約 (17.2.2 手順3-4) ==\n");

    std::wstring base = MakeBatchTree();
    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    exts.push_back(L".h");

    BatchSearchResult bs;
    std::wstring err;
    bool ok = BatchProcessor::SearchFolder(base, exts, true, L"old_function();",
                                           L"auto", BatchProgressFn(), bs, err);
    Check(ok, L"一括検索が成功する");
    Check(bs.scanned_count == 3, L"走査ファイル数 3 を報告する");
    Check(bs.hit_file_count == 3, L"ヒットしたファイル数 3 を報告する");
    Check(bs.total_matches == 4, L"ヒット件数 4 を集約する");
    Check(bs.entries.size() == 3, L"ヒットしたファイルのみ entries に保持する");
    Check(bs.matches.size() == 4, L"一致箇所を 1 つの一覧へ集約する");

    bool ids_ok = true;
    for (size_t i = 0; i < bs.matches.size(); ++i)
        if (bs.matches[i].match_id != (int)i + 1) ids_ok = false;
    Check(ids_ok, L"一致箇所に全体通し番号を振る");

    bool files_ok = true;
    for (size_t i = 0; i < bs.matches.size(); ++i)
        if (bs.matches[i].file_path.empty()) files_ok = false;
    Check(files_ok, L"各一致箇所にファイルパスが設定される");

    // ヒットなし
    BatchSearchResult none;
    BatchProcessor::SearchFolder(base, exts, true, L"no_such_symbol_xyz",
                                 L"auto", BatchProgressFn(), none, err);
    Check(none.total_matches == 0, L"ヒットがない場合は 0 件を返す");

    // 対象なし
    std::vector<std::wstring> noext;
    noext.push_back(L".rs");
    BatchSearchResult empty;
    bool ok2 = BatchProcessor::SearchFolder(base, noext, true, L"old_function();",
                                            L"auto", BatchProgressFn(), empty, err);
    Check(!ok2 && !err.empty(), L"条件に一致するファイルが無い場合にエラーを返す");
}

static void TestBatchDryRunAndExecute()
{
    Out(L"\n== 一括ドライラン / 一括変換 (17.2.2 手順6-8) ==\n");

    std::wstring base = MakeBatchTree();
    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    exts.push_back(L".h");

    std::wstring a_path = util::JoinPath(base, L"a.c");
    std::wstring b_path = util::JoinPath(base, L"b.c");
    std::wstring c_path = util::JoinPath(util::JoinPath(base, L"sub"), L"c.h");

    std::wstring orig_a, orig_b, orig_c;
    FileInfo fi;
    FileManager::ReadText(a_path, L"auto", orig_a, fi);
    FileManager::ReadText(b_path, L"auto", orig_b, fi);
    FileManager::ReadText(c_path, L"auto", orig_c, fi);

    OperationContext ctx;
    ctx.operation_id = L"OP_TEST_BATCH";
    ctx.switch_name = L"MIGRATION_001";
    ctx.old_code = L"old_function();";
    ctx.new_code = L"new_function();";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.auto_indent = true;
    ctx.create_backup = true;
    ctx.target_folder = base;
    ctx.started_at = util::NowDateTime();

    // ---- ドライラン ----
    {
        BatchSearchResult bs;
        std::wstring err;
        BatchProcessor::SearchFolder(base, exts, true, L"old_function();", L"auto",
                                     BatchProgressFn(), bs, err);
        std::vector<std::wstring> blocks;
        for (size_t z = 0; z < bs.matches.size(); ++z) bs.matches[z].selected = true;
    BuildBlocks(bs.entries, bs.matches, blocks);

        ctx.execution_mode = ExecutionMode::DryRun;
        BatchExecuteResult br;
        std::vector<UndoRecord> ur;
        bool ok = BatchProcessor::ExecuteBatch(ctx, L"auto", bs.entries, bs.matches,
                                               blocks, L"", BatchProgressFn(), br, ur, err);
        Check(ok, L"一括ドライランが成功する");
        Check(br.replaced_count == 4, L"ドライランで置換予定 4 件を報告する");
        Check(ur.empty(), L"ドライランでは Undo 情報を作成しない");

        std::wstring now_a;
        FileManager::ReadText(a_path, L"auto", now_a, fi);
        Check(now_a == orig_a, L"ドライランではファイルが変更されない");

        bool no_switch = (now_a.find(L"#ifdef") == std::wstring::npos);
        Check(no_switch, L"ドライラン後もコンパイルスイッチが挿入されていない");
    }

    // ---- 本実行 ----
    BatchExecuteResult real;
    std::vector<UndoRecord> ur2;
    std::wstring backup_dir = util::JoinPath(TestDir(), L"batch_backup");
    util::EnsureDirectory(backup_dir);
    CleanDir(backup_dir);
    {
        BatchSearchResult bs;
        std::wstring err;
        BatchProcessor::SearchFolder(base, exts, true, L"old_function();", L"auto",
                                     BatchProgressFn(), bs, err);
        std::vector<std::wstring> blocks;
        for (size_t z = 0; z < bs.matches.size(); ++z) bs.matches[z].selected = true;
    BuildBlocks(bs.entries, bs.matches, blocks);

        ctx.execution_mode = ExecutionMode::Real;
        bool ok = BatchProcessor::ExecuteBatch(ctx, L"auto", bs.entries, bs.matches,
                                               blocks, backup_dir, BatchProgressFn(),
                                               real, ur2, err);
        Check(ok, L"一括変換が成功する");
        Check(real.replaced_count == 4, L"一括変換で 4 件を置換する");
        Check(real.error_count == 0, L"失敗ファイルが 0 件である");
        Check(ur2.size() == 3, L"成功した 3 ファイル分の Undo 情報を作成する");
        Check(real.status == L"Success", L"結果ステータスが Success である");
    }

    // ファイル内容の検証
    {
        std::wstring now_a, now_b, now_c;
        FileInfo fa, fb, fc;
        FileManager::ReadText(a_path, L"auto", now_a, fa);
        FileManager::ReadText(b_path, L"auto", now_b, fb);
        FileManager::ReadText(c_path, L"auto", now_c, fc);

        Check(now_a.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
              L"a.c にコンパイルスイッチが挿入される");
        int cnt = 0;
        size_t pos = 0;
        while ((pos = now_a.find(L"#ifdef MIGRATION_001", pos)) != std::wstring::npos) { ++cnt; pos += 4; }
        Check(cnt == 2, L"a.c の 2 箇所すべてが置換される");
        Check(now_a.find(L"        new_function();") != std::wstring::npos,
              L"8 スペースのインデントが対象行から引き継がれる（6.8）");
        Check(now_a.find(L"    new_function();") != std::wstring::npos,
              L"4 スペースのインデントが対象行から引き継がれる（6.8）");
        Check(now_b.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
              L"b.c にコンパイルスイッチが挿入される");
        Check(now_c.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
              L"サブフォルダの c.h にコンパイルスイッチが挿入される");
        Check(now_b.find(L"old_function();") != std::wstring::npos,
              L"旧コードが #else 側に保持される");
    }

    // バックアップの検証
    {
        WIN32_FIND_DATAW fd;
        std::wstring pat = util::JoinPath(backup_dir, L"*.bak");
        HANDLE h = ::FindFirstFileW(pat.c_str(), &fd);
        int n = 0;
        if (h != INVALID_HANDLE_VALUE) {
            do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) ++n; }
            while (::FindNextFileW(h, &fd));
            ::FindClose(h);
        }
        Check(n == 3, L"一括バックアップが 3 ファイル分作成される");
    }

    // ---- 統合レポート ----
    {
        std::vector<ReportFileSection> sections;
        for (size_t i = 0; i < ur2.size(); ++i) {
            ReportFileSection s;
            s.file_path = ur2[i].target_file_path;
            FileInfo sf;
            FileManager::ReadText(s.file_path, L"auto", s.before_text, sf);
            s.after_text = s.before_text;
            s.encoding_name = EncodingDetector::EncodingName(sf.encoding);
            s.newline_name = L"LF";
            s.result.status = L"Success";
            s.result.replaced_count = 1;
            sections.push_back(s);
        }

        std::wstring path = DiffReportGenerator::BuildIntegratedReportPath(
            TestDir(), base, util::TimestampFileName());
        Check(path.find(L"_batch_diff.html") != std::wstring::npos,
              L"統合レポートのファイル名が <stamp>_<folder>_batch_diff.html 形式である");

        std::wstring err;
        Check(DiffReportGenerator::GenerateIntegrated(path, ctx, sections, real, err),
              L"統合レポートを生成する");
        Check(util::PathExists(path), L"統合レポートファイルが実在する");

        std::string raw;
        ReadRaw(path, raw);
        std::wstring html = util::Utf8ToWide(raw.size() > 3 ? raw.substr(3) : raw);
        Check(html.find(L"Migration Switch Report (一括処理)") != std::wstring::npos,
              L"統合レポートのタイトルが含まれる");
        Check(html.find(L"ファイル別サマリ") != std::wstring::npos,
              L"ファイル別サマリ表が含まれる");
        Check(html.find(L"a.c") != std::wstring::npos &&
              html.find(L"b.c") != std::wstring::npos &&
              html.find(L"c.h") != std::wstring::npos,
              L"全対象ファイルがレポートに含まれる");
        Check(html.find(L"MIGRATION_001") != std::wstring::npos,
              L"スイッチ名がレポートに含まれる");
        Check(html.find(L"拡張子フィルタ") != std::wstring::npos,
              L"拡張子フィルタの情報が含まれる");
    }

    // ---- 一括 Undo ----
    {
        UndoManager um;
        um.SetRecords(ur2);
        Check(um.FileCount() == 3, L"一括 Undo の対象ファイル数 3 を保持する");
        Check(um.CanUndo(), L"一括 Undo が可能と判定する");

        std::wstring uerr;
        std::vector<std::wstring> sidecars;
        Check(um.Undo(uerr, sidecars), L"一括 Undo が成功する");
        Check(sidecars.size() == 3, L"復元前に 3 ファイルを退避する");

        std::wstring now_a, now_b, now_c;
        FileInfo fa, fb, fc;
        FileManager::ReadText(a_path, L"auto", now_a, fa);
        FileManager::ReadText(b_path, L"auto", now_b, fb);
        FileManager::ReadText(c_path, L"auto", now_c, fc);
        Check(now_a == orig_a, L"a.c が実行前の内容へ復元される");
        Check(now_b == orig_b, L"b.c が実行前の内容へ復元される");
        Check(now_c == orig_c, L"c.h が実行前の内容へ復元される");
        Check(!um.CanUndo(), L"一括 Undo 後は再実行できない");
    }
}

static void TestBatchSafety()
{
    Out(L"\n== 一括処理の安全性 (17章 / 19章) ==\n");

    std::wstring base = MakeBatchTree();
    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    exts.push_back(L".h");

    BatchSearchResult bs;
    std::wstring err;
    BatchProcessor::SearchFolder(base, exts, true, L"old_function();", L"auto",
                                 BatchProgressFn(), bs, err);
    std::vector<std::wstring> blocks;
    for (size_t z = 0; z < bs.matches.size(); ++z) bs.matches[z].selected = true;
    BuildBlocks(bs.entries, bs.matches, blocks);

    // 検索後にファイルが変更された場合
    std::wstring a_path = util::JoinPath(base, L"a.c");
    WriteTextFile(a_path, L"int changed_after_search(void)\n{\n    return 0;\n}\n");

    // ファイル単位の選択解除（1 ファイルを対象外にする）
    bs.entries[1].selected = false;   // b.c を対象外にする

    OperationContext ctx;
    ctx.operation_id = L"OP_TEST_SAFETY";
    ctx.switch_name = L"MIGRATION_001";
    ctx.old_code = L"old_function();";
    ctx.new_code = L"new_function();";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.auto_indent = false;
    ctx.create_backup = true;
    ctx.target_folder = base;
    ctx.execution_mode = ExecutionMode::Real;
    ctx.started_at = util::NowDateTime();

    std::wstring bdir2 = util::JoinPath(TestDir(), L"batch_backup2");
    util::EnsureDirectory(bdir2);
    CleanDir(bdir2);

    BatchExecuteResult br;
    std::vector<UndoRecord> ur;
    BatchProcessor::ExecuteBatch(ctx, L"auto", bs.entries, bs.matches, blocks,
                                 bdir2,
                                 BatchProgressFn(), br, ur, err);

    bool changed_detected = false;
    bool excluded_detected = false;
    for (size_t i = 0; i < br.files.size(); ++i) {
        if (br.files[i].status == L"Skipped" &&
            br.files[i].error.find(L"変更") != std::wstring::npos) changed_detected = true;
        if (br.files[i].status == L"Skipped" &&
            br.files[i].error.find(L"選択されていません") != std::wstring::npos) excluded_detected = true;
    }
    Check(changed_detected, L"検索後に変更されたファイルを検出してスキップする");
    Check(excluded_detected, L"対象外にしたファイルはスキップする");
    Check(br.skipped_count == 2, L"スキップ件数 2 を報告する");
    Check(br.status != L"Success", L"スキップがある場合に Success 以外のステータスを返す");
}


// =============================================================================
// スイッチ定義ヘッダの一元管理
// =============================================================================
static void TestSwitchHeaderManager()
{
    Out(L"\n== スイッチ定義ヘッダ管理 ==\n");

    // ---- 名前・パス ----
    std::wstring err;
    Check(SwitchHeaderManager::DefaultHeaderName() == L"migration_switch.h",
          L"既定のヘッダ名は migration_switch.h");
    Check(SwitchHeaderManager::ValidateHeaderName(L"migration_switch.h", err),
          L"正常なヘッダ名を許可する");
    Check(!SwitchHeaderManager::ValidateHeaderName(L"migration_switch", err),
          L"拡張子 .h 以外を拒否する");
    Check(!SwitchHeaderManager::ValidateHeaderName(L"bad<name>.h", err),
          L"ファイル名に使えない文字を拒否する");
    Check(SwitchHeaderManager::GuardFromName(L"migration_switch.h") == L"MIGRATION_SWITCH_H",
          L"インクルードガード名を生成する");

    std::wstring base = util::JoinPath(TestDir(), L"swh");
    util::EnsureDirectory(base);
    std::wstring hp = SwitchHeaderManager::ResolveHeaderPath(base, L"", L"migration_switch.h");
    Check(hp == util::JoinPath(base, L"migration_switch.h"),
          L"出力先未指定なら対象と同一フォルダへ解決する");

    Check(SwitchHeaderManager::MakeIncludePath(util::JoinPath(base, L"a.c"), hp) ==
          L"migration_switch.h",
          L"同一フォルダのソースにはファイル名のみの #include を返す");
    std::wstring subdir = util::JoinPath(base, L"sub");
    util::EnsureDirectory(subdir);
    Check(SwitchHeaderManager::MakeIncludePath(util::JoinPath(subdir, L"c.h"), hp) ==
          L"../migration_switch.h",
          L"サブフォルダのソースには相対パスの #include を返す");

    // ---- 新規作成 ----
    std::wstring header = util::JoinPath(base, L"migration_switch.h");
    if (util::PathExists(header)) FileManager::RemoveFile(header);

    SwitchHeaderInfo info;
    bool added = false;
    std::vector<std::wstring> srcs;
    srcs.push_back(util::JoinPath(base, L"a.c"));
    Check(SwitchHeaderManager::RegisterSwitch(header, L"MIGRATION_001", L"API 差し替え",
                                              L"old_function();", L"new_function();",
                                              srcs, info, added, err),
          L"スイッチ定義ヘッダを新規作成する");
    Check(added, L"新規スイッチとして登録される");
    Check(util::PathExists(header), L"ヘッダファイルが実在する");
    Check(info.entries.size() == 1 && info.entries[0].name == L"MIGRATION_001",
          L"登録したスイッチが 1 件読み込める");
    Check(!info.entries[0].enabled, L"新規スイッチの既定は無効（旧コードのまま）");

    std::string raw;
    ReadRaw(header, raw);
    std::wstring text = util::NormalizeNewlines(util::Utf8ToWide(raw));
    Check(text.find(L"#ifndef MIGRATION_SWITCH_H") != std::wstring::npos,
          L"インクルードガードが出力される");
    Check(text.find(L"CopyMigration Switcher managed section: BEGIN") != std::wstring::npos &&
          text.find(L"CopyMigration Switcher managed section: END") != std::wstring::npos,
          L"管理領域のマーカーが出力される");
    Check(text.find(L"/* #define MIGRATION_001 */") != std::wstring::npos,
          L"無効スイッチはコメントアウトされた #define で出力される");
    Check(text.find(L"old_function(); -> new_function();") != std::wstring::npos,
          L"旧→新の要約がコメントに含まれる");
    Check(text.find(L"手動定義領域") != std::wstring::npos,
          L"手動定義領域が用意される");

    // ---- 冪等性（重複しない）----
    SwitchHeaderInfo info2;
    bool added2 = true;
    Check(SwitchHeaderManager::RegisterSwitch(header, L"MIGRATION_001", L"API 差し替え",
                                              L"old_function();", L"new_function();",
                                              srcs, info2, added2, err),
          L"同じスイッチを再登録できる");
    Check(!added2, L"既存スイッチは新規追加されない（冪等）");
    Check(info2.entries.size() == 1, L"スイッチが重複登録されない");

    // ---- 複数ファイルの集約 ----
    SwitchHeaderInfo info3;
    bool added3 = false;
    std::vector<std::wstring> srcs2;
    srcs2.push_back(util::JoinPath(base, L"a.c"));
    srcs2.push_back(util::JoinPath(base, L"b.c"));
    SwitchHeaderManager::RegisterSwitch(header, L"MIGRATION_001", L"API 差し替え",
                                        L"old_function();", L"new_function();",
                                        srcs2, info3, added3, err);
    Check(info3.entries.size() == 1 && info3.entries[0].files.size() == 2,
          L"複数ファイルを 1 スイッチへ集約できる（重複ファイルは増やさない）");

    // ---- 複数スイッチ ----
    SwitchHeaderInfo info4;
    bool added4 = false;
    SwitchHeaderManager::RegisterSwitch(header, L"USE_NEW_LOGIC", L"ロジック更新",
                                        L"calc();", L"calc_v2();", srcs, info4, added4, err);
    Check(info4.entries.size() == 2, L"複数スイッチを管理できる");
    Check(info4.entries[0].name == L"MIGRATION_001" &&
          info4.entries[1].name == L"USE_NEW_LOGIC",
          L"スイッチは名前順に整列される");

    // ---- 有効化／無効化 ----
    SwitchHeaderInfo info5;
    Check(SwitchHeaderManager::SetEnabled(header, L"MIGRATION_001", true, info5, err),
          L"スイッチを有効化できる");
    Check(info5.entries[0].enabled, L"有効化が反映される");
    ReadRaw(header, raw);
    text = util::NormalizeNewlines(util::Utf8ToWide(raw));
    Check(text.find(L"\n#define MIGRATION_001\n") != std::wstring::npos,
          L"有効スイッチはコメントなしの #define で出力される");

    SwitchHeaderInfo info6;
    Check(SwitchHeaderManager::SetEnabled(header, L"MIGRATION_001", false, info6, err),
          L"スイッチを無効化できる");
    Check(!info6.entries[0].enabled, L"無効化が反映される");

    // ---- 手動編集の保持 ----
    {
        ReadRaw(header, raw);
        std::wstring t2 = util::Utf8ToWide(raw);
        t2 = util::ReplaceAll(t2, L"/* ---- 手動定義領域（この領域はツールが書き換えません） ---- */",
                              L"/* ---- 手動定義領域（この領域はツールが書き換えません） ---- */\n#define MY_MANUAL_SWITCH 1");
        std::string bytes = util::WideToUtf8(t2);
        WriteRaw(header, bytes);
    }
    SwitchHeaderInfo info7;
    bool added7 = false;
    SwitchHeaderManager::RegisterSwitch(header, L"THIRD_SWITCH", L"3 つ目",
                                        L"a();", L"b();", srcs, info7, added7, err);
    ReadRaw(header, raw);
    text = util::NormalizeNewlines(util::Utf8ToWide(raw));
    Check(text.find(L"#define MY_MANUAL_SWITCH 1") != std::wstring::npos,
          L"手動定義領域の内容が更新後も保持される");
    Check(text.find(L"THIRD_SWITCH") != std::wstring::npos,
          L"管理領域へ新しいスイッチを追記できる");

    // ---- 削除 ----
    SwitchHeaderInfo info8;
    Check(SwitchHeaderManager::RemoveSwitch(header, L"THIRD_SWITCH", info8, err),
          L"スイッチを削除できる");
    Check(info8.entries.size() == 2, L"削除後は件数が減る");
    SwitchHeaderInfo info9;
    std::wstring e2;
    Check(!SwitchHeaderManager::RemoveSwitch(header, L"NO_SUCH_SWITCH", info9, e2),
          L"存在しないスイッチの削除はエラーになる");

    // ---- #include 挿入 ----
    {
        std::wstring t, e;
        bool a = false; int line = 0;

        t = L"#include <stdio.h>\n#include <string.h>\n\nint x;\n";
        Check(SwitchHeaderManager::EnsureInclude(t, L"migration_switch.h", a, line, e),
              L"#include を追加できる（既存 include あり）");
        Check(a && line == 3, L"最後の #include の直後に挿入される（3 行目）");
        Check(t.find(L"#include <string.h>\n#include \"migration_switch.h\"") != std::wstring::npos,
              L"既存 include の後ろに正しく挿入される");

        t = L"#ifndef SECOND_C\n#define SECOND_C\n\nint y;\n";
        a = false; line = 0;
        SwitchHeaderManager::EnsureInclude(t, L"migration_switch.h", a, line, e);
        Check(a && line == 3, L"インクルードガードの直後に挿入される");

        t = L"/* third.c\n * サンプル\n */\nint z;\n";
        a = false; line = 0;
        SwitchHeaderManager::EnsureInclude(t, L"migration_switch.h", a, line, e);
        Check(a && line == 4, L"先頭のブロックコメントの後ろに挿入される");

        t = L"int w;\n";
        a = false; line = 0;
        SwitchHeaderManager::EnsureInclude(t, L"migration_switch.h", a, line, e);
        Check(a && line == 1, L"include もガードも無い場合は先頭に挿入される");

        a = false; line = 0;
        SwitchHeaderManager::EnsureInclude(t, L"migration_switch.h", a, line, e);
        Check(!a, L"既に #include がある場合は重複追加しない（冪等）");
        Check(SwitchHeaderManager::HasInclude(t, L"migration_switch.h"),
              L"HasInclude が追加済みを検出する");
        Check(!SwitchHeaderManager::HasInclude(t, L"other.h"),
              L"HasInclude が未追加を検出する");

        // 改行・末尾の保持
        std::wstring t2 = L"int v;\n";
        a = false; line = 0;
        SwitchHeaderManager::EnsureInclude(t2, L"migration_switch.h", a, line, e);
        Check(t2.size() > 0 && t2[t2.size() - 1] == L'\n',
              L"末尾の改行が保持される");
    }

    // ---- 要約 ----
    Check(SwitchHeaderManager::MakeSummary(L"old_function();", L"new_function();") ==
          L"old_function(); -> new_function();",
          L"要約文字列を生成する");
    Check(SwitchHeaderManager::MakeSummary(L"", L"") == L"(旧コード未入力) -> (新コード未入力)",
          L"未入力時の要約を生成する");
}

// =============================================================================
// 実ソースへの統合（ヘッダ生成 + #include + スイッチ挿入）
// =============================================================================
// テストで使う作業フォルダ。
//   環境変数 MS_VERIFY_DIR があればそこを使い、無ければ %TEMP%\ms_verify を使う。
static std::wstring VerifyDir()
{
    wchar_t buf[1024];
    DWORD n = ::GetEnvironmentVariableW(L"MS_VERIFY_DIR", buf, 1024);
    std::wstring dir;
    if (n > 0 && n < 1024) {
        dir = buf;
    } else {
        wchar_t tmp[MAX_PATH];
        DWORD m = ::GetTempPathW(MAX_PATH, tmp);
        std::wstring base = (m > 0) ? std::wstring(tmp) : util::ModuleDirectory();
        dir = util::JoinPath(base, L"ms_verify");
    }
    util::EnsureDirectory(dir);
    return dir;
}

static void TestSwitchHeaderIntegration()
{
    Out(L"\n== 実ソースへの統合（ヘッダ + #include + スイッチ）==\n");

    std::wstring dir = VerifyDir();
    std::wstring header = util::JoinPath(dir, L"migration_switch.h");
    std::wstring f1 = util::JoinPath(dir, L"sample.c");
    std::wstring f2 = util::JoinPath(dir, L"second.c");
    std::wstring f3 = util::JoinPath(dir, L"third.c");

    FileManager::RemoveFile(header);
    FileManager::RemoveFile(f1);
    FileManager::RemoveFile(f2);
    FileManager::RemoveFile(f3);

    WriteTextFile(f1,
        L"#include <stdio.h>\n"
        L"#include <stdlib.h>\n"
        L"\n"
        L"static int old_function(int x) { return x + 1; }\n"
        L"static int new_function(int x) { return x + 2; }\n"
        L"\n"
        L"int sample_use(int x)\n"
        L"{\n"
        L"    return old_function(x);\n"
        L"}\n"
        L"\n"
        L"int main(void)\n"
        L"{\n"
        L"    printf(\"%d\\n\", sample_use(1));\n"
        L"    return 0;\n"
        L"}\n");
    WriteTextFile(f2,
        L"#ifndef SECOND_C\n"
        L"#define SECOND_C\n"
        L"\n"
        L"static int old_function(int x) { return x + 1; }\n"
        L"static int new_function(int x) { return x + 2; }\n"
        L"\n"
        L"int second_use(int x)\n"
        L"{\n"
        L"    return old_function(x);\n"
        L"}\n"
        L"\n"
        L"#endif\n");
    WriteTextFile(f3,
        L"/* third.c\n"
        L" * マイグレーション対象のサンプル\n"
        L" */\n"
        L"static int old_function(int x) { return x + 1; }\n"
        L"static int new_function(int x) { return x + 2; }\n"
        L"\n"
        L"int third_use(int x)\n"
        L"{\n"
        L"    return old_function(x);\n"
        L"}\n");

    // ---- 一括検索 ----
    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    BatchSearchResult bs;
    std::wstring err;
    BatchProcessor::SearchFolder(dir, exts, false, L"old_function(x)", L"auto",
                                 BatchProgressFn(), bs, err);
    Check(bs.hit_file_count == 3, L"3 ファイルでヒットする");
    Check(bs.total_matches == 3, L"3 件ヒットする");
    for (size_t i = 0; i < bs.matches.size(); ++i) bs.matches[i].selected = true;

    std::vector<std::wstring> blocks;
    blocks.assign(bs.matches.size(), std::wstring());
    for (size_t i = 0; i < bs.matches.size(); ++i) {
        std::wstring src_text;
        for (size_t k = 0; k < bs.entries.size(); ++k)
            if (bs.entries[k].path == bs.matches[i].file_path) { src_text = bs.entries[k].text; break; }
        std::vector<std::wstring> w;
        std::wstring hint = SearchEngine::IndentHintAt(src_text, bs.matches[i]);
        blocks[i] = SwitchGenerator::GenerateEx(
            SwitchStyle::IfdefNewFirst, L"MIGRATION_001",
            L"old_function(x)", L"new_function(x)", false, L"", hint, w);
    }

    // ---- スイッチ定義ヘッダの生成 ----
    std::vector<std::wstring> srcs;
    srcs.push_back(f1); srcs.push_back(f2); srcs.push_back(f3);
    SwitchHeaderInfo hi;
    bool added = false;
    Check(SwitchHeaderManager::RegisterSwitch(header, L"MIGRATION_001",
                                              L"old_function を new_function へ差し替え",
                                              L"old_function(x)", L"new_function(x)",
                                              srcs, hi, added, err),
          L"実ソースからスイッチ定義ヘッダを生成する");
    Check(added && hi.entries.size() == 1 && hi.entries[0].files.size() == 3,
          L"3 ファイルがスイッチへ登録される");

    // ---- 一括変換（#include 追加 + スイッチ挿入）----
    OperationContext ctx;
    ctx.operation_id = L"OP_SWH";
    ctx.switch_name = L"MIGRATION_001";
    ctx.old_code = L"old_function(x)";
    ctx.new_code = L"new_function(x)";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.auto_indent = false;
    ctx.create_backup = true;
    ctx.target_folder = dir;
    ctx.execution_mode = ExecutionMode::Real;
    ctx.started_at = util::NowDateTime();
    ctx.manage_switch_header = true;
    ctx.add_switch_header_include = true;
    ctx.switch_header_path = header;

    BatchExecuteResult br;
    std::vector<UndoRecord> ur;
    Check(BatchProcessor::ExecuteBatch(ctx, L"auto", bs.entries, bs.matches, blocks,
                                       util::JoinPath(dir, L"backup"), BatchProgressFn(),
                                       br, ur, err),
          L"一括変換が成功する");
    Check(br.replaced_count == 3, L"3 件を置換する");
    Check(br.error_count == 0, L"失敗ファイルが無い");

    // ---- 生成物の検証 ----
    std::wstring t1, t2, t3;
    FileInfo fi;
    FileManager::ReadText(f1, L"auto", t1, fi); t1 = util::NormalizeNewlines(t1);
    FileManager::ReadText(f2, L"auto", t2, fi); t2 = util::NormalizeNewlines(t2);
    FileManager::ReadText(f3, L"auto", t3, fi); t3 = util::NormalizeNewlines(t3);

    Check(t1.find(L"#include \"migration_switch.h\"") != std::wstring::npos,
          L"sample.c に #include が追加される");
    Check(t1.find(L"#include <stdlib.h>\n#include \"migration_switch.h\"") != std::wstring::npos,
          L"sample.c の #include は既存 include の直後に挿入される");
    Check(t2.find(L"#define SECOND_C\n#include \"migration_switch.h\"") != std::wstring::npos,
          L"second.c の #include はインクルードガード直後に挿入される");
    Check(t3.find(L" */\n#include \"migration_switch.h\"") != std::wstring::npos,
          L"third.c の #include は先頭コメントの後ろに挿入される");

    // 行中一致（"    return old_function(x);"）は行頭ホイストされ、
    // #ifdef/#else/#endif が行頭に来て前後のコードが各分岐へ複製される（C として妥当）
    Check(t1.find(L"#ifdef MIGRATION_001\n    return new_function(x);\n"
                  L"#else\n    return old_function(x);\n#endif") != std::wstring::npos,
          L"sample.c にコンパイルスイッチブロックが行頭ホイストで挿入される");
    Check(t1.find(L"    return #ifdef") == std::wstring::npos,
          L"#ifdef が行中に残らない（C として不正にならない）");
    Check(t2.find(L"#ifdef MIGRATION_001") != std::wstring::npos &&
          t3.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
          L"second.c / third.c にもスイッチが挿入される");

    bool inc_all = true;
    for (size_t i = 0; i < br.files.size(); ++i) {
        bool found = false;
        for (size_t k = 0; k < bs.entries.size(); ++k)
            if (bs.entries[k].path == br.files[i].file_path) found = bs.entries[k].include_added;
        if (!found) inc_all = false;
    }
    Check(inc_all, L"全ファイルで #include 追加が記録される");

    std::string hraw;
    ReadRaw(header, hraw);
    std::wstring htext = util::NormalizeNewlines(util::Utf8ToWide(hraw));
    Check(htext.find(L"/* #define MIGRATION_001 */") != std::wstring::npos,
          L"生成されたヘッダではスイッチが無効（旧コードが既定）");

    Out(L"  [INFO] 検証用ファイル出力先: " + dir + L"\n");
}

// =============================================================================
// ヘッダを含む Undo（新規作成されたヘッダは削除して戻す）
// =============================================================================
static void TestSwitchHeaderUndo()
{
    Out(L"\n== スイッチ定義ヘッダの Undo ==\n");

    std::wstring dir = util::JoinPath(VerifyDir(), L"undo_case");
    util::EnsureDirectory(dir);
    CleanDir(dir);

    std::wstring header = util::JoinPath(dir, L"migration_switch.h");
    std::wstring f1 = util::JoinPath(dir, L"undo_sample.c");

    std::wstring original =
        L"#include <stdio.h>\n"
        L"static int old_function(int x) { return x + 1; }\n"
        L"static int new_function(int x) { return x + 2; }\n"
        L"int undo_use(int x)\n"
        L"{\n"
        L"    return old_function(x);\n"
        L"}\n";
    WriteTextFile(f1, original);
    Check(!util::PathExists(header), L"開始時点でヘッダは存在しない");

    // 1) スイッチ定義ヘッダを新規作成
    SwitchHeaderInfo hi;
    bool added = false;
    std::wstring err;
    std::vector<std::wstring> srcs;
    srcs.push_back(f1);
    Check(SwitchHeaderManager::RegisterSwitch(header, L"MIGRATION_001", L"Undo 検証",
                                              L"old_function(x)", L"new_function(x)",
                                              srcs, hi, added, err),
          L"スイッチ定義ヘッダを新規作成する");
    Check(added && util::PathExists(header), L"ヘッダが新規作成される");

    std::wstring bdir = util::JoinPath(dir, L"backup");
    std::wstring bpath;
    Check(BackupManager::CreateBackup(f1, bdir, bpath, err),
          L"変換前のソースをバックアップする");

    // 2) ソースを変換（#include 追加 + スイッチ挿入）
    MatchResult mm;
    mm.match_id = 1;
    mm.file_path = f1;
    size_t pos = original.find(L"old_function(x)");
    mm.start_offset = pos;
    mm.end_offset = pos + wcslen(L"old_function(x)");
    mm.start_line = 6;
    mm.end_line = 6;
    mm.matched_text = L"old_function(x)";
    mm.selected = true;

    std::vector<std::wstring> w;
    std::wstring block = SwitchGenerator::Generate(SwitchStyle::IfdefNewFirst,
                                                   L"MIGRATION_001",
                                                   L"old_function(x)", L"new_function(x)",
                                                   false, L"", w);
    std::vector<MatchResult> ms;
    ms.push_back(mm);
    std::vector<std::wstring> blk;
    blk.push_back(block);

    std::wstring text = PreviewGenerator::BuildAfterTextEach(original, ms, blk);
    bool inc = false;
    int inc_line = 0;
    SwitchHeaderManager::EnsureInclude(text, L"migration_switch.h", inc, inc_line, err);
    Check(inc, L"#include が追加される");

    FileInfo fi = FileManager::Inspect(f1, L"auto");
    std::wstring werr;
    FileManager::WriteTextAtomic(f1, text, fi.encoding, fi.has_bom, fi.newline, werr);

    std::wstring changed;
    FileManager::ReadText(f1, L"auto", changed, fi);
    changed = util::NormalizeNewlines(changed);
    Check(changed.find(L"#include \"migration_switch.h\"") != std::wstring::npos,
          L"変換後は #include が入っている");
    Check(changed.find(L"#ifdef MIGRATION_001") != std::wstring::npos,
          L"変換後はスイッチが入っている");
    Check(changed.find(L"    return #ifdef") == std::wstring::npos,
          L"変換後も #ifdef は行頭にある");

    // 3) Undo（新規作成されたヘッダは削除で戻す）
    UndoRecord frec;
    frec.operation_id = L"OP_UNDO";
    frec.target_file_path = f1;
    frec.backup_file_path = bpath;
    frec.valid = true;

    UndoRecord hrec;
    hrec.operation_id = L"OP_UNDO";
    hrec.target_file_path = header;
    hrec.backup_file_path.clear();
    hrec.remove_on_undo = true;
    hrec.valid = true;

    UndoManager um;
    std::vector<UndoRecord> recs;
    recs.push_back(frec);
    recs.push_back(hrec);
    um.SetRecords(recs);
    Check(um.FileCount() == 2, L"Undo 対象が 2 件（ソース + ヘッダ）");
    Check(um.CanUndo(), L"新規ヘッダを含む操作も Undo 可能と判定する");

    std::wstring uerr;
    std::vector<std::wstring> sidecars;
    Check(um.Undo(uerr, sidecars), L"Undo が成功する");
    Check(sidecars.size() == 2, L"復元前に 2 件を退避する");

    std::wstring now;
    FileManager::ReadText(f1, L"auto", now, fi);
    now = util::NormalizeNewlines(now);
    Check(now == util::NormalizeNewlines(original),
          L"ソースが実行前の内容へ完全に復元される（#include も取り除かれる）");
    Check(!util::PathExists(header),
          L"新規作成されたスイッチ定義ヘッダが削除される");
}


// =============================================================================
// AI 連携（仕様書生成）
// =============================================================================
static void TestAiClientBasics()
{
    Out(L"\n== AI クライアント（JSON / SSE 解析）==\n");

    // JSON エスケープ
    std::wstring esc = AiClient::JsonEscape(L"a\"b\\c\nd\te");
    Check(esc == L"a\\\"b\\\\c\\nd\\te", L"JSON 文字列をエスケープする");
    std::wstring uni = AiClient::JsonEscape(L"改行\x01");
    Check(uni.find(L"\\u0001") != std::wstring::npos, L"制御文字を \\uXXXX へ変換する");

    // リクエスト本文
    AiSettings st;
    st.api_key = L"k";
    st.task_type = L"super_agent";
    std::string body = AiClient::BuildRequestBody(st, L"テスト\"改行\n含み");
    std::string bs(body.begin(), body.end());
    Check(bs.find("\"message\":\"") != std::string::npos, L"リクエスト本文に message を含む");
    Check(bs.find("\\\"") != std::string::npos, L"引用符がエスケープされる");
    Check(bs.find("\\n") != std::string::npos, L"改行がエスケープされる");
    Check(bs.find("\"task_type\":\"super_agent\"") != std::string::npos, L"task_type を送信する");
    AiSettings st2 = st;
    st2.model = L"m1";
    std::string body2 = AiClient::BuildRequestBody(st2, L"x");
    Check(std::string(body2.begin(), body2.end()).find("\"use_model\":\"m1\"") != std::string::npos,
          L"モデル指定時は use_model を送信する");

    // 文字コード変換（日本語の往復）
    std::wstring jp = L"変更仕様書テスト";
    Check(AiClient::Utf8Decode(AiClient::Utf8Encode(jp)) == jp, L"UTF-8 の往復で文字化けしない");

    // SSE 解析: delta の連結
    std::wstring acc;
    const char* lines[] = {
        "{\"version\":1,\"project_id\":\"p1\"}",
        "{\"version\":1,\"message\":\"Still processing...\",\"heartbeat\":1}",
        "{\"version\":1,\"delta\":\"\\u5909\"}",
        "{\"version\":1,\"delta\":\"\\u66f4\"}",
        "{\"version\":1,\"status\":\"ok\",\"message\":\"success\",\"data\":{\"result_content\":{\"last_message\":[\"\\u5909\\u66f4\\u4ed5\\u69d8\\u66f8\"]}}}"
    };
    bool got_final = false;
    std::wstring final_text;
    for (int i = 0; i < 5; ++i) {
        SseEvent ev = AiClient::ParseSseLine(lines[i]);
        if (ev.has_delta) acc += ev.delta;
        if (ev.has_final) { got_final = true; final_text = ev.final_text; }
    }
    Check(acc == L"変更", L"SSE の delta を連結できる");
    Check(got_final && final_text == L"変更仕様書", L"完了イベントから本文を取り出せる");
    Check(AiClient::ParseSseLine("data: {\"delta\":\"x\"}").delta == L"x",
          L"data: プレフィックス付きの行も解析できる");
    Check(AiClient::ParseSseLine("").has_delta == false, L"空行は無視される");
    // 生の UTF-8（\u エスケープされていない日本語）を含む SSE。
    // ソースは CP932 で保存されるため、バイト列を明示して検証する。
    std::string raw_utf8 =
        "{\"version\":1,\"delta\":\"\xe5\xa4\x89\xe6\x9b\xb4\"}";   // 変更
    SseEvent ev_u = AiClient::ParseSseLine(raw_utf8);
    Check(ev_u.has_delta && ev_u.delta == std::wstring(L"\u5909\u66f4"),
          L"エスケープされていない UTF-8 の delta も正しく解釈する");
    std::string raw_final =
        "{\"status\":\"ok\",\"data\":{\"result_content\":{\"last_message\":["
        "\"\xe5\xa4\x89\xe6\x9b\xb4\xe4\xbb\x95\xe6\xa7\x98\xe6\x9b\xb8\"]}}}";  // 変更仕様書
    Check(AiClient::ParseSseLine(raw_final).final_text ==
              std::wstring(L"\u5909\u66f4\u4ed5\u69d8\u66f8"),
          L"完了イベントの本文も UTF-8 として解釈する");
    std::string raw_mixed = "{\"delta\":\"a\\u3042b\"}";   // a + \u3042 + b
    std::wstring want;
    want += L'a'; want += (wchar_t)0x3042; want += L'b';
    Check(AiClient::ParseSseLine(raw_mixed).delta == want,
          L"ASCII と \\u エスケープの混在を処理する");
    Check(AiClient::DefaultEndpoint().find(L"/api/tool_cli/agent_ask") != std::wstring::npos,
          L"既定エンドポイントが Genspark の API を指す");
}

static void TestSpecGenerator()
{
    Out(L"\n== 仕様書生成（プロンプト / Markdown→HTML）==\n");

    SpecInput in;
    in.system_name = L"在庫管理システム";
    in.overview = L"C/S 構成の基幹システム";
    in.purpose = L"旧 API のサポート終了対応";
    in.modification = L"stock_alloc() を new_stock_alloc() へ差し替える";
    in.switch_name = L"MIGRATION_001";
    in.old_code = L"stock_alloc(item, qty);";
    in.new_code = L"new_stock_alloc(item, qty, ctx);";
    in.target_summary = L"sample.c（1 箇所）";
    in.created_at = L"2026-10-18 15:30:00";

    std::wstring pc = SpecGenerator::BuildChangeSpecPrompt(in);
    Check(pc.find(L"在庫管理システム") != std::wstring::npos, L"変更仕様書プロンプトにシステム名を含む");
    Check(pc.find(L"変更仕様書") != std::wstring::npos, L"変更仕様書の作成を指示する");
    Check(pc.find(L"## 5. コンパイルスイッチ仕様") != std::wstring::npos, L"スイッチ仕様の章を指示する");
    Check(pc.find(L"## 8. ロールバック手順") != std::wstring::npos, L"ロールバック手順の章を指示する");
    Check(pc.find(L"stock_alloc(item, qty);") != std::wstring::npos, L"旧コードをプロンプトへ埋め込む");

    std::wstring pi = SpecGenerator::BuildInspectionSpecPrompt(in);
    Check(pi.find(L"検査仕様書") != std::wstring::npos, L"検査仕様書の作成を指示する");
    Check(pi.find(L"## 3. 検査観点一覧") != std::wstring::npos, L"検査観点の章を指示する");
    Check(pi.find(L"## 6. スイッチ切替の確認項目") != std::wstring::npos, L"スイッチ切替の確認章を指示する");

    // ---- API を使わない運用: AI チャットへ貼る依頼文 txt ----
    std::wstring txt = SpecGenerator::BuildPromptTxt(SPEC_CHANGE, in);
    Check(txt.find(L"変更仕様書 作成依頼") != std::wstring::npos,
          L"依頼文 txt に見出しを付ける");
    Check(txt.find(L"【このファイルの使い方】") != std::wstring::npos,
          L"依頼文 txt に使い方を記載する");
    Check(txt.find(L"AI チャット") != std::wstring::npos,
          L"AI チャットへ貼る旨を記載する");
    Check(txt.find(L"在庫管理システム") != std::wstring::npos,
          L"依頼文 txt にシステム情報を埋め込む");
    Check(txt.find(L"## 9. 未確定事項・確認事項") != std::wstring::npos,
          L"依頼文 txt に章立ての指示を含む");
    Check(txt.find(L"出力の厳守事項") != std::wstring::npos,
          L"依頼文 txt に出力形式の指示を含む");
    Check(txt.find(L"回答を貼り戻すときのメモ") != std::wstring::npos,
          L"依頼文 txt に貼り戻しのメモを含む");
    Check(txt.find(L"\r\n") != std::wstring::npos &&
          txt.find(L"\n\n") == std::wstring::npos,
          L"依頼文 txt の改行を CRLF に統一する");

    std::wstring txt2 = SpecGenerator::BuildPromptTxt(SPEC_INSPECTION, in);
    Check(txt2.find(L"検査仕様書 作成依頼") != std::wstring::npos &&
          txt2.find(L"## 3. 検査観点一覧") != std::wstring::npos,
          L"検査仕様書の依頼文 txt を生成する");
    Check(SpecGenerator::MakePromptFileName(L"在庫管理システム", SPEC_CHANGE,
                                             L"2026-10-18 15:40:00") ==
          L"在庫管理システム_changespec_prompt_20261018154000.txt",
          L"依頼文 txt のファイル名を組み立てる");

    // 貼り付けた回答 → レポート化（API を使わない経路）
    std::wstring dir2 = util::JoinPath(VerifyDir(), L"spec_paste");
    util::EnsureDirectory(dir2);
    std::wstring pasted =
        L"## 1. 検査目的\n在庫引当の差し替えが正しく動作することを確認する。\n\n"
        L"| No | 手順 | 期待結果 |\n"
        L"|----|------|----------|\n"
        L"| 1 | スイッチ OFF で実行 | 旧コードが動作する |\n"
        L"| 2 | スイッチ ON で実行 | 新コードが動作する |\n";

    // 1) 依頼文 txt をファイルへ出力（ExportPrompts と同じ経路）
    std::wstring tpath = util::JoinPath(dir2, SpecGenerator::MakePromptFileName(
        in.system_name, SPEC_CHANGE, L"2026-10-18 15:40:00"));
    std::wstring werr2;
    Check(SpecGenerator::WriteReportFile(tpath, SpecGenerator::BuildPromptTxt(SPEC_CHANGE, in), werr2),
          L"依頼文 txt をファイルへ書き出せる");
    std::string traw;
    ReadRaw(tpath, traw);
    Check(traw.size() > 3 && (unsigned char)traw[0] == 0xEF,
          L"依頼文 txt は UTF-8 BOM 付きで出力される");
    std::wstring tback = util::Utf8ToWide(traw);
    Check(tback.find(L"変更仕様書 作成依頼") != std::wstring::npos &&
          tback.find(L"在庫管理システム") != std::wstring::npos,
          L"出力した依頼文 txt に依頼内容とシステム情報が含まれる");
    Check(tback.find(L"\r\n") != std::wstring::npos,
          L"出力した依頼文 txt の改行が CRLF である");

    // 2) 貼り付けた回答をレポート化
    std::wstring rpath = util::JoinPath(dir2,
        SpecGenerator::MakeReportFileName(in.system_name, SPEC_INSPECTION, L"2026-10-18 15:41:00"));
    std::wstring rhtml = SpecGenerator::WrapReportHtml(
        L"検査仕様書 - " + in.system_name, SpecGenerator::MarkdownToHtml(pasted), in);
    Check(SpecGenerator::WriteReportFile(rpath, rhtml, werr2),
          L"貼り付けた回答を HTML レポートへ書き出せる");
    Check(util::PathExists(rpath), L"貼り付け由来のレポートが存在する");
    std::string rraw;
    ReadRaw(rpath, rraw);
    std::wstring rback = util::Utf8ToWide(rraw);
    Check(rback.find(L"<table>") != std::wstring::npos &&
          rback.find(L"スイッチ OFF で実行") != std::wstring::npos,
          L"貼り付けた Markdown の表がレポートへ反映される");

    // Markdown → HTML
    std::wstring md =
        L"## 1. 変更概要\n"
        L"在庫引当の**呼び出し**を差し替える。\n\n"
        L"- 項目1\n"
        L"- 項目2\n\n"
        L"| ID | 観点 | 期待結果 |\n"
        L"|----|------|----------|\n"
        L"| T1 | 引当 | 成功 |\n"
        L"| T2 | 在庫不足 | エラー |\n\n"
        L"```\nold_call();\n```\n";
    std::wstring html = SpecGenerator::MarkdownToHtml(md);
    Check(html.find(L"<h2>1. 変更概要</h2>") != std::wstring::npos, L"見出しを h2 へ変換する");
    Check(html.find(L"<strong>呼び出し</strong>") != std::wstring::npos, L"強調を strong へ変換する");
    Check(html.find(L"<li>項目1</li>") != std::wstring::npos, L"箇条書きを ul/li へ変換する");
    Check(html.find(L"<table>") != std::wstring::npos &&
          html.find(L"<th>観点</th>") != std::wstring::npos &&
          html.find(L"<td>在庫不足</td>") != std::wstring::npos, L"表を table へ変換する");
    Check(html.find(L"<pre><code>old_call();") != std::wstring::npos, L"コードブロックを pre/code へ変換する");
    Check(SpecGenerator::MarkdownToHtml(L"<script>alert(1)</script>").find(L"&lt;script&gt;") != std::wstring::npos,
          L"HTML をエスケープする");

    // ファイル名 / レポート書き出し
    Check(SpecGenerator::SanitizeFileName(L"在庫 管理:システム/v1") == L"在庫_管理システムv1",
          L"ファイル名に使えない文字を除去する");
    Check(SpecGenerator::MakeReportFileName(L"在庫管理システム", SPEC_INSPECTION,
                                             L"2026-10-18 15:30:00") ==
          L"在庫管理システム_inspectspec_20261018153000.html",
          L"レポート名を組み立てる");
    Check(SpecGenerator::KindLabel(SPEC_CHANGE) == L"変更仕様書" &&
          SpecGenerator::KindLabel(SPEC_INSPECTION) == L"検査仕様書", L"仕様書の名称を返す");

    std::wstring dir = util::JoinPath(VerifyDir(), L"spec_case");
    util::EnsureDirectory(dir);
    std::wstring path = util::JoinPath(dir, L"unit_report.html");
    std::wstring content = SpecGenerator::WrapReportHtml(L"変更仕様書 - 在庫管理システム", html, in);
    std::wstring werr;
    Check(SpecGenerator::WriteReportFile(path, content, werr), L"HTML レポートを書き出せる");
    Check(util::PathExists(path), L"レポートファイルが存在する");
    std::string raw;
    ReadRaw(path, raw);
    Check(raw.size() > 3 && (unsigned char)raw[0] == 0xEF, L"レポートは UTF-8 BOM 付きで出力される");
    std::wstring back = util::Utf8ToWide(raw);
    Check(back.find(L"<meta charset=\"utf-8\">") != std::wstring::npos, L"レポートに charset を埋め込む");
    Check(back.find(L"在庫管理システム") != std::wstring::npos, L"レポートにシステム名を含む");
    Check(back.find(L"MIGRATION_001") != std::wstring::npos, L"レポートにスイッチ名を含む");
    Check(back.find(L"AI が生成した草案") != std::wstring::npos, L"AI 生成である旨の注記を含む");
}

static void TestAiConfigRoundtrip()
{
    Out(L"\n== AI 連携 / システム情報の設定保存 ==\n");

    std::wstring dir = util::JoinPath(VerifyDir(), L"ai_cfg");
    util::EnsureDirectory(dir);
    std::wstring ini = util::JoinPath(dir, L"config.ini");
    FileManager::RemoveFile(ini);

    ConfigManager cm;
    cm.SetPath(ini);
    AppConfig c;
    c.ai_api_key = L"gsk-dummy-key";
    c.ai_endpoint = AiClient::DefaultEndpoint();
    c.ai_model = L"model-x";
    c.ai_task_type = L"super_agent";
    c.ai_timeout_sec = 480;
    c.ai_open_report = false;
    c.sys_name = L"在庫管理システム";
    c.sys_overview = L"概要テキスト";
    c.sys_purpose = L"目的テキスト";
    c.sys_modification = L"修正箇所テキスト";
    c.sys_extra = L"追加指示テキスト";
    c.last_spec_report_path = L"reports\\x.html";
    cm.Save(c);

    AppConfig d;
    cm.Load(d);
    Check(d.ai_api_key == L"gsk-dummy-key", L"API キーを保存/復元できる");
    Check(d.ai_endpoint.find(L"genspark.ai") != std::wstring::npos, L"エンドポイントを保存/復元できる");
    Check(d.ai_model == L"model-x", L"モデルを保存/復元できる");
    Check(d.ai_timeout_sec == 480, L"タイムアウトを保存/復元できる");
    Check(d.ai_open_report == false, L"レポート自動オープン設定を保存/復元できる");
    Check(d.sys_name == L"在庫管理システム", L"システム名を保存/復元できる");
    Check(d.sys_overview == L"概要テキスト", L"システム概要を保存/復元できる");
    Check(d.sys_purpose == L"目的テキスト", L"目的を保存/復元できる");
    Check(d.sys_modification == L"修正箇所テキスト", L"修正箇所概要を保存/復元できる");
    Check(d.sys_extra == L"追加指示テキスト", L"追加指示を保存/復元できる");
}

static void TestAiOpenAiCompat()
{
    Out(L"\n== OpenAI 互換 API 対応 ==\n");

    // URL の組み立て
    Check(AiClient::DefaultOpenAiEndpoint().find(L"/chat/completions") != std::wstring::npos,
          L"既定の OpenAI 互換エンドポイントを返す");
    Check(AiClient::ResolveOpenAiUrl(L"https://api.openai.com/v1") ==
          L"https://api.openai.com/v1/chat/completions",
          L"ベース URL に /chat/completions を補う");
    Check(AiClient::ResolveOpenAiUrl(L"https://api.openai.com/v1/") ==
          L"https://api.openai.com/v1/chat/completions",
          L"末尾スラッシュがあっても正しく補う");
    Check(AiClient::ResolveOpenAiUrl(L"https://x.example/v1/chat/completions") ==
          L"https://x.example/v1/chat/completions",
          L"完全な URL はそのまま使う");
    Check(AiClient::ResolveOpenAiUrl(L"https://r.openai.azure.com/openai/deployments/d/chat/completions?api-version=2024-06-01")
              .find(L"api-version") != std::wstring::npos,
          L"Azure 形式（api-version 付き）をそのまま使う");
    Check(AiClient::ResolveOpenAiUrl(L"") == AiClient::DefaultOpenAiEndpoint(),
          L"未入力なら既定へフォールバックする");

    // リクエスト本文
    AiSettings st;
    st.provider = AiProvider::OpenAiCompat;
    st.model = L"gpt-4o-mini";
    std::string body = AiClient::BuildOpenAiRequestBody(st, L"テスト\"と改行\nを含む");
    std::string b(body.begin(), body.end());
    Check(b.find("\"model\":\"gpt-4o-mini\"") != std::string::npos,
          L"リクエスト本文にモデル名を含む");
    Check(b.find("\"role\":\"user\"") != std::string::npos &&
          b.find("\"messages\":[") != std::string::npos,
          L"messages / role を組み立てる");
    Check(b.find("\"stream\":false") != std::string::npos,
          L"非ストリーミングを指定する");
    Check(b.find("\\\"") != std::string::npos && b.find("\\n") != std::string::npos,
          L"引用符と改行をエスケープする");

    // 応答の解析（\u エスケープ形式）
    std::string ok1 = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"\\u5909\\u66f4\\u4ed5\\u69d8\\u66f8\"}}]}";
    std::wstring t1, e1;
    Check(AiClient::ParseOpenAiResponse(ok1, t1, e1) && t1 == std::wstring(L"\u5909\u66f4\u4ed5\u69d8\u66f8"),
          L"choices[0].message.content を取り出せる");
    // 応答の解析（生の UTF-8、ソースは CP932 なのでバイト列で指定）
    std::string ok2 = "{\"choices\":[{\"message\":{\"content\":\"\xe5\xa4\x89\xe6\x9b\xb4\"}}]}";
    std::wstring t2, e2;
    Check(AiClient::ParseOpenAiResponse(ok2, t2, e2) && t2 == std::wstring(L"\u5909\u66f4"),
          L"エスケープされていない UTF-8 の本文も解釈する");
    // 旧 completions 形式
    std::string ok3 = "{\"choices\":[{\"text\":\"hello\"}]}";
    std::wstring t3, e3;
    Check(AiClient::ParseOpenAiResponse(ok3, t3, e3) && t3 == L"hello",
          L"choices[0].text 形式にも対応する");
    // エラー応答
    std::string ng = "{\"error\":{\"message\":\"Incorrect API key provided\",\"type\":\"invalid_request_error\"}}";
    std::wstring t4, e4;
    Check(!AiClient::ParseOpenAiResponse(ng, t4, e4) &&
          e4.find(L"Incorrect API key") != std::wstring::npos,
          L"エラー応答からメッセージを取り出す");
    // 空応答
    std::wstring t5, e5;
    Check(!AiClient::ParseOpenAiResponse("", t5, e5) && !e5.empty(),
          L"空の応答をエラーとして扱う");

    // 設定の保存/復元
    std::wstring dir = util::JoinPath(VerifyDir(), L"ai_openai_cfg");
    util::EnsureDirectory(dir);
    std::wstring ini = util::JoinPath(dir, L"config.ini");
    FileManager::RemoveFile(ini);
    ConfigManager cm;
    cm.SetPath(ini);
    AppConfig c;
    c.ai_provider = 1;
    c.ai_openai_key = L"sk-test-key";
    c.ai_openai_endpoint = L"https://api.openai.com/v1";
    c.ai_openai_model = L"gpt-4o-mini";
    c.ai_api_key = L"gsk-keep";
    cm.Save(c);
    AppConfig d;
    cm.Load(d);
    Check(d.ai_provider == 1, L"API 種別を保存/復元できる");
    Check(d.ai_openai_key == L"sk-test-key", L"OpenAI 互換の API キーを保存/復元できる");
    Check(d.ai_openai_endpoint == L"https://api.openai.com/v1", L"OpenAI 互換のエンドポイントを保存/復元できる");
    Check(d.ai_openai_model == L"gpt-4o-mini", L"OpenAI 互換のモデルを保存/復元できる");
    Check(d.ai_api_key == L"gsk-keep", L"Genspark 側の設定が消えない");
}

static void TestCodeScope()
{
    Out(L"\n== 関数単位検索（CodeScope） ==\n");
    std::wstring src =
        L"int other(void) { return 0; }\n"
        L"\n"
        L"int stock_use(int item, int qty)\n"
        L"{\n"
        L"    int rc = stock_alloc(item, qty);\n"
        L"    if (rc) { stock_alloc(item, qty); }\n"
        L"    return rc;\n"
        L"}\n"
        L"\n"
        L"int tail_use(int x)\n"
        L"{\n"
        L"    return stock_alloc(x, 1);\n"
        L"}\n";

    int s = 0, e = 0;
    std::wstring err;
    Check(CodeScope::FindFunctionBody(src, L"stock_use", s, e, err), L"関数本体を検出できる");
    Check(s == 4 && e == 8, L"関数本体の行範囲が正しい（4～8 行）");
    Check(!CodeScope::FindFunctionBody(src, L"no_such_fn", s, e, err), L"存在しない関数は失敗する");
    Check(!err.empty(), L"失敗時に理由を返す");
    Check(!CodeScope::FindFunctionBody(src, L"stock", s, e, err),
          L"識別子の一部（stock）には誤一致しない");

    std::vector<MatchResult> r =
        SearchEngine::SearchInFunction(src, L"stock_alloc", L"t.c", L"stock_use", err);
    Check(r.size() == 2, L"関数内の一致だけを 2 件返す");
    for (size_t i = 0; i < r.size(); ++i)
        Check(r[i].start_line >= 4 && r[i].start_line <= 8, L"関数外の一致を含まない");
}

static void TestSearchOptions()
{
    Out(L"\n== 類似一致 / 指定行範囲検索 ==\n");
    Check(SearchEngine::NormalizeForSimilar(L"  int  rc = f( a , b );  ") == L"int rc = f( a , b );",
          L"連続する空白を 1 つに畳む");
    Check(SearchEngine::SimilarityPercent(L"abcde", L"abcde") == 100, L"同一文字列の類似度は 100%");
    Check(SearchEngine::SimilarityPercent(L"abcde", L"abcde") >
          SearchEngine::SimilarityPercent(L"abcde", L"xyzwv"),
          L"似ている文字列の方が類似度が高い");

    std::wstring text =
        L"int a(void)\n"
        L"{\n"
        L"    int rc = stock_alloc(item, qty);\n"
        L"    int rc2 = stock_alloc(item,  qty);\n"
        L"    int rc3 = totally_different_call(x);\n"
        L"}\n";

    std::vector<MatchResult> sim =
        SearchEngine::SearchSimilar(text, L"stock_alloc(item, qty)", L"t.c", 70);
    Check(sim.size() == 1, L"空白だけが異なる行を 1 件だけ検出する（完全一致行は除外）");
    if (!sim.empty()) {
        Check(sim[0].match_type == MatchType::Similar, L"一致種別が類似一致になる");
        Check(sim[0].match_rate >= 70, L"一致率がしきい値以上になる");
        Check(sim[0].start_line == 4, L"類似一致の行番号が正しい");
        Check(sim[0].matched_text.find(L"stock_alloc") != std::wstring::npos,
              L"一致文字列を元の表記で返す");
        Check(sim[0].status == MatchStatus::Warning, L"類似一致は警告状態になる");
    }
    Check(SearchEngine::SearchSimilar(text, L"stock_alloc(item, qty)", L"t.c", 10).size() >=
          sim.size(), L"しきい値を下げると検出件数は減らない");

    std::vector<MatchResult> rng =
        SearchEngine::SearchInRange(text, L"stock_alloc", L"t.c", 1, 3);
    Check(rng.size() == 1, L"指定行範囲（1～3 行）で 1 件だけ検出する");
    if (!rng.empty()) Check(rng[0].start_line == 3, L"範囲内の行だけを返す");
    Check(SearchEngine::SearchInRange(text, L"stock_alloc", L"t.c", 4, 0).size() == 1,
          L"終了行 0 は最終行までを意味する");
    Check(SearchEngine::SearchInRange(text, L"stock_alloc", L"t.c", 10, 20).empty(),
          L"範囲外には一致しない");
}

static void TestEucJp()
{
    Out(L"\n== EUC-JP 対応 ==\n");
    // コードページ 51932 は kernel32 に存在しないため、内蔵テーブルで判定・変換する。
    Check(EncodingDetector::IsEucJpAvailable(), L"EUC-JP は内蔵テーブルで常に利用可能");

    const unsigned char bytes[] = { 0xA4,0xA2, 0xA4,0xA4, 0xA4,0xA6, 0xA4,0xA8, 0xA4,0xAA };
    std::string euc((const char*)bytes, sizeof(bytes));
    const std::wstring expect = L"\u3042\u3044\u3046\u3048\u304A";

    Check(EncodingDetector::IsValidEucJp(euc), L"EUC-JP のひらがなを妥当と判定する");
    size_t bom = 0;
    Check(EncodingDetector::Detect(euc, bom) == TextEncoding::EucJp,
          L"CP932 では半角カタカナになるバイト列を EUC-JP と自動判定する");

    std::wstring t; TextEncoding enc;
    Check(EncodingDetector::Decode(euc, L"auto", t, enc) && t == expect,
          L"EUC-JP を自動判定して読み込める");
    std::wstring t2; TextEncoding enc2;
    Check(EncodingDetector::Decode(euc, L"eucjp", t2, enc2) && enc2 == TextEncoding::EucJp && t2 == expect,
          L"強制モード eucjp で読み込める");
    std::string back;
    Check(EncodingDetector::Encode(expect, TextEncoding::EucJp, false, back) && back == euc,
          L"EUC-JP へ再エンコードすると元のバイト列に戻る");
    Check(std::wstring(EncodingDetector::EncodingName(TextEncoding::EucJp)) == L"EUC-JP",
          L"文字コード名を返す");

    // 回帰: verify_kit/euc_sample.c 11 行目相当（CP932 では不正、EUC-JP では日本語コメント）
    const unsigned char line[] = { 0xB5,0xEC, 0x20, 0x41,0x50,0x49, 0x20,
                                   0xA4,0xF2, 0xB8,0xC6, 0xA4,0xD3, 0xBD,0xD0, 0xA4,0xB9 };
    std::string eucLine((const char*)line, sizeof(line));
    Check(!EncodingDetector::IsValidShiftJis(eucLine), L"EUC-JP の日本語コメントは CP932 としては不正");
    Check(EncodingDetector::IsValidEucJp(eucLine), L"EUC-JP としては妥当");
    Check(EncodingDetector::Detect(eucLine, bom) == TextEncoding::EucJp,
          L"CP932 として不正な EUC-JP 行を EUC-JP と判定する（文字化けの回帰）");
    std::wstring lt; TextEncoding lenc;
    Check(EncodingDetector::Decode(eucLine, L"auto", lt, lenc) && lt == L"\u65E7\u0020\u0041\u0050\u0049\u0020\u3092\u547C\u3073\u51FA\u3059",
          L"EUC-JP の日本語コメントが文字化けせずに読める");

    // 半角カタカナ（0x8E リード）
    const unsigned char hk[] = { 0x8E,0xB1, 0x8E,0xB2, 0x8E,0xB3 };
    std::string hks((const char*)hk, sizeof(hk));
    std::wstring ht; TextEncoding henc;
    Check(EncodingDetector::IsValidEucJp(hks) &&
          EncodingDetector::Decode(hks, L"eucjp", ht, henc) && ht == L"\uFF71\uFF72\uFF73",
          L"半角カタカナ（0x8E リード）を強制モード eucjp で読み込める");

    // JIS X 0212（0x8F リード）
    const unsigned char j212[] = { 0x8F, 0xB0, 0xA1 };
    std::string s212((const char*)j212, sizeof(j212));
    std::wstring t212; TextEncoding e212;
    Check(EncodingDetector::Decode(s212, L"eucjp", t212, e212) && t212 == L"\u4E02",
          L"JIS X 0212（0x8F リード）を読み込める");

    std::wstring dir = util::JoinPath(VerifyDir(), L"eucjp");
    util::EnsureDirectory(dir);
    std::wstring fp = util::JoinPath(dir, L"euc.txt");
    std::wstring werr;
    Check(FileManager::WriteTextAtomic(fp, expect, TextEncoding::EucJp, false,
                                       NewlineKind::CRLF, werr),
          L"EUC-JP でファイルへ書き出せる");
    std::wstring rt; FileInfo fi;
    Check(FileManager::ReadText(fp, L"auto", rt, fi) && fi.encoding == TextEncoding::EucJp && rt == expect,
          L"書き出した EUC-JP ファイルを自動判定で読み戻せる");
    std::wstring rt2; FileInfo fi2;
    Check(FileManager::ReadText(fp, L"eucjp", rt2, fi2) && rt2 == expect,
          L"強制モード eucjp でファイルを読み戻せる");
}

static void TestMultiGenerationUndo()
{
    Out(L"\n== 複数世代 Undo ==\n");
    std::wstring dir = util::JoinPath(VerifyDir(), L"undo_gen");
    util::EnsureDirectory(dir);
    std::wstring target = util::JoinPath(dir, L"gen.txt");
    std::wstring bak1   = util::JoinPath(dir, L"gen.txt.1.bak");
    std::wstring bak2   = util::JoinPath(dir, L"gen.txt.2.bak");
    std::wstring werr;
    FileManager::WriteTextAtomic(bak1, L"v1", TextEncoding::UTF8, false, NewlineKind::LF, werr);
    FileManager::WriteTextAtomic(bak2, L"v2", TextEncoding::UTF8, false, NewlineKind::LF, werr);
    FileManager::WriteTextAtomic(target, L"v3", TextEncoding::UTF8, false, NewlineKind::LF, werr);

    UndoManager u;
    u.SetMaxGenerations(10);
    UndoRecord r1;
    r1.operation_id = L"OP_1";
    r1.target_file_path = target;
    r1.backup_file_path = bak1;
    r1.executed_at = L"2026-01-01 00:00:00";
    u.SetRecord(r1);

    UndoRecord r2;
    r2.operation_id = L"OP_2";
    r2.target_file_path = target;
    r2.backup_file_path = bak2;
    r2.executed_at = L"2026-01-01 00:01:00";
    u.SetRecord(r2);

    Check(u.HistorySize() == 2, L"2 世代を保持する");
    Check(u.FileCount() == 1, L"最新世代のファイル数は 1");
    Check(u.GenerationLabel(0).find(L"OP_2") != std::wstring::npos, L"最新世代のラベルを返す");

    std::vector<std::wstring> sc;
    std::wstring err;
    Check(u.Undo(err, sc), L"1 回目の Undo が成功する");
    std::wstring a; FileInfo afi;
    FileManager::ReadText(target, L"auto", a, afi);
    Check(util::Trim(a) == L"v2", L"1 回目の Undo で 1 世代前（v2）へ戻る");
    Check(u.HistorySize() == 1 && u.CanUndo(), L"1 世代残るので続けて Undo できる");

    Check(u.Undo(err, sc), L"2 回目の Undo が成功する");
    std::wstring b; FileInfo bfi;
    FileManager::ReadText(target, L"auto", b, bfi);
    Check(util::Trim(b) == L"v1", L"2 回目の Undo でさらに前（v1）へ戻る");
    Check(u.HistorySize() == 0 && !u.CanUndo(), L"履歴を使い切ると Undo できない");
    Check(!u.Undo(err, sc) && !err.empty(), L"履歴が無いときは理由を返して失敗する");

    u.SetMaxGenerations(2);
    for (int i = 0; i < 3; ++i) {
        UndoRecord r;
        r.target_file_path = target;
        r.backup_file_path = bak1;
        u.SetRecord(r);
    }
    Check(u.HistorySize() == 2, L"上限を超えた古い世代は破棄される");
    Check(u.MaxGenerations() == 2, L"保持世代数を取得できる");
}

static void TestReportFormats()
{
    Out(L"\n== Markdown / JSON レポート ==\n");
    OperationContext ctx;
    ctx.operation_id = L"OP_TEST_0001";
    ctx.target_file_path = L"C:\\work\\sample.c";
    ctx.switch_name = L"MIGRATION_001";
    ctx.old_code = L"stock_alloc(item, qty)";
    ctx.new_code = L"new_stock_alloc(item, qty, 0)";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.encoding = TextEncoding::ShiftJIS;
    ctx.newline = NewlineKind::CRLF;
    ctx.started_at = L"2026-01-01 00:00:00";

    std::wstring before = L"int a;\n    stock_alloc(item, qty);\n";
    std::wstring after  = L"int a;\n#ifdef MIGRATION_001\n    new_stock_alloc(item, qty, 0);\n#else\n    stock_alloc(item, qty);\n#endif\n";
    std::vector<MatchResult> ms = SearchEngine::Search(before, ctx.old_code, ctx.target_file_path);
    Check(ms.size() == 1, L"レポート用の一致を 1 件用意できる");

    ReplaceResult res;
    res.operation_id = ctx.operation_id;
    res.target_file_path = ctx.target_file_path;
    res.status = L"Success";
    res.replaced_count = (int)ms.size();
    res.switch_header_path = L"C:\\work\\migration_switch.h";
    res.switch_header_updated = true;

    std::wstring md = DiffReportGenerator::BuildMarkdown(ctx, ms, before, after, res);
    Check(md.find(L"# 変更差分レポート") == 0, L"Markdown は見出しから始まる");
    Check(md.find(L"MIGRATION_001") != std::wstring::npos, L"Markdown にスイッチ名を出力する");
    Check(md.find(L"| No |") != std::wstring::npos, L"Markdown に一致箇所の表を出力する");
    Check(md.find(L"## 変換前") != std::wstring::npos &&
          md.find(L"## 変換後") != std::wstring::npos, L"Markdown に変換前後を出力する");

    std::wstring js = DiffReportGenerator::BuildJson(ctx, ms, before, after, res);
    Check(js.find(L"\"operation_id\": \"OP_TEST_0001\"") != std::wstring::npos,
          L"JSON に operation_id を出力する");
    Check(js.find(L"\"matches\"") != std::wstring::npos, L"JSON に matches 配列を出力する");
    Check(js.find(L"\"switch_header_updated\": true") != std::wstring::npos,
          L"JSON に真偽値を出力する");
    Check(js.find(L"\\n") != std::wstring::npos, L"JSON の改行をエスケープする");

    Check(DiffReportGenerator::JsonEscape(L"a\"b\\c\nd\te") == L"a\\\"b\\\\c\\nd\\te",
          L"JSON 文字列をエスケープする");

    std::wstring dir = util::JoinPath(VerifyDir(), L"report_fmt");
    util::EnsureDirectory(dir);
    std::wstring mdp = util::JoinPath(dir, L"test_diff.md");
    std::wstring jsp = util::JoinPath(dir, L"test_diff.json");
    std::wstring err;
    Check(DiffReportGenerator::GenerateMarkdown(mdp, ctx, ms, before, after, res, err),
          L"Markdown レポートを出力できる");
    Check(DiffReportGenerator::GenerateJson(jsp, ctx, ms, before, after, res, err),
          L"JSON レポートを出力できる");
    Check(util::PathExists(mdp) && util::PathExists(jsp), L"レポートファイルが作成される");

    std::wstring mt; FileInfo mfi;
    Check(FileManager::ReadText(mdp, L"auto", mt, mfi) && mt.find(L"# 変更差分レポート") == 0,
          L"Markdown レポートを読み戻せる");
    Check(mfi.encoding == TextEncoding::UTF8BOM, L"Markdown は UTF-8 (BOM) で出力する");
    std::wstring jt; FileInfo jfi;
    Check(FileManager::ReadText(jsp, L"auto", jt, jfi) && jt.find(L"\"matches\"") != std::wstring::npos,
          L"JSON レポートを読み戻せる");
    Check(jfi.encoding == TextEncoding::UTF8, L"JSON は BOM なし UTF-8 で出力する");

    Check(DiffReportGenerator::BuildReportPathExt(L"reports", L"C:\\work\\sample.c",
                                                  L"20260101_000000", L".md")
              .find(L"20260101_000000_sample_diff.md") != std::wstring::npos,
          L"Markdown レポートのファイル名を組み立てる");
}

static void TestResultExporter()
{
    Out(L"\n== 検索結果エクスポート（CSV / Markdown / JSON） ==\n");
    Check(ResultExporter::CsvEscape(L"abc") == L"abc", L"CSV: 通常の文字列はそのまま");
    Check(ResultExporter::CsvEscape(L"a,b") == L"\"a,b\"", L"CSV: カンマを引用符で囲む");
    Check(ResultExporter::CsvEscape(L"a\"b") == L"\"a\"\"b\"", L"CSV: 引用符を二重化する");
    Check(ResultExporter::CsvEscape(L"a\nb") == L"\"a\nb\"", L"CSV: 改行を引用符で囲む");

    std::vector<MatchResult> ms = SearchEngine::Search(
        L"int a;\n    stock_alloc(item, qty);\n    stock_alloc(item, qty);\n",
        L"stock_alloc(item, qty)", L"C:\\work\\sample.c");
    Check(ms.size() == 2, L"エクスポート用の一致を 2 件用意できる");
    ms[0].selected = true;

    std::wstring csv = ResultExporter::BuildCsv(ms, L"OP_TEST_0001");
    Check(csv.find(L"ファイル名") != std::wstring::npos, L"CSV にヘッダ行を出力する");
    Check(csv.find(L"sample.c") != std::wstring::npos, L"CSV にファイル名を出力する");
    Check(util::SplitLines(csv).size() == ms.size() + 1, L"CSV の行数がヘッダ + 件数になる");

    std::wstring md = ResultExporter::BuildMarkdown(ms, L"OP_TEST_0001");
    Check(md.find(L"OP_TEST_0001") != std::wstring::npos, L"Markdown に操作 ID を出力する");
    Check(md.find(L"|---:") != std::wstring::npos, L"Markdown に表の区切りを出力する");

    std::wstring js = ResultExporter::BuildJson(ms, L"OP_TEST_0001");
    Check(js.find(L"\"total\": 2") != std::wstring::npos, L"JSON に件数を出力する");
    Check(js.find(L"\"report_type\": \"search_result\"") != std::wstring::npos,
          L"JSON にレポート種別を出力する");

    std::wstring dir = util::JoinPath(VerifyDir(), L"export");
    util::EnsureDirectory(dir);
    std::wstring err;
    std::wstring p1 = util::JoinPath(dir, L"result.csv");
    std::wstring p2 = util::JoinPath(dir, L"result.md");
    std::wstring p3 = util::JoinPath(dir, L"result.json");
    Check(ResultExporter::Export(p1, ms, L"OP_TEST_0001", ExportFormat::Csv, L"utf8bom", err),
          L"CSV を出力できる");
    Check(ResultExporter::Export(p2, ms, L"OP_TEST_0001", ExportFormat::Markdown, L"utf8bom", err),
          L"Markdown を出力できる");
    Check(ResultExporter::Export(p3, ms, L"OP_TEST_0001", ExportFormat::Json, L"utf8bom", err),
          L"JSON を出力できる");
    Check(util::PathExists(p1) && util::PathExists(p2) && util::PathExists(p3),
          L"3 形式のファイルが作成される");

    std::wstring rt; FileInfo fi;
    Check(FileManager::ReadText(p1, L"auto", rt, fi) && rt.find(L"ファイル名") != std::wstring::npos,
          L"CSV を読み戻せる");
    Check(fi.encoding == TextEncoding::UTF8BOM, L"CSV は UTF-8 (BOM) で出力する（Excel 向け）");
    std::wstring jt; FileInfo jfi;
    Check(FileManager::ReadText(p3, L"auto", jt, jfi) && jfi.encoding == TextEncoding::UTF8,
          L"JSON は BOM なし UTF-8 で出力する");

    std::wstring ep = ResultExporter::BuildExportPath(L"reports", L"20260101_000000",
                                                      ExportFormat::Csv, L"sample");
    Check(ep.find(L"20260101_000000_sample_result.csv") != std::wstring::npos,
          L"エクスポートファイル名を組み立てる");
}

static void TestConfigRoundtripExt()
{
    Out(L"\n== 設定の往復（検索条件 / レポート形式 / Undo 世代 / EUC-JP） ==\n");
    std::wstring dir = util::JoinPath(VerifyDir(), L"cfg_ext");
    util::EnsureDirectory(dir);
    std::wstring ini = util::JoinPath(dir, L"config.ini");
    FileManager::RemoveFile(ini);

    ConfigManager cm;
    cm.SetPath(ini);
    AppConfig c;
    c.search_similar = true;
    c.search_similar_threshold = 85;
    c.search_line_range = true;
    c.search_line_from = 10;
    c.search_line_to = 200;
    c.search_function = true;
    c.search_function_name = L"stock_use";
    c.report_markdown = true;
    c.report_json = true;
    c.undo_max_generations = 25;
    c.encoding_mode = L"eucjp";
    cm.Save(c);

    AppConfig d;
    cm.Load(d);
    Check(d.search_similar && d.search_similar_threshold == 85, L"類似一致検索の設定を保存/復元できる");
    Check(d.search_line_range && d.search_line_from == 10 && d.search_line_to == 200,
          L"指定行範囲の設定を保存/復元できる");
    Check(d.search_function && d.search_function_name == L"stock_use",
          L"関数単位検索の設定を保存/復元できる");
    Check(d.report_markdown && d.report_json, L"レポート形式の設定を保存/復元できる");
    Check(d.undo_max_generations == 25, L"Undo 世代数の設定を保存/復元できる");
    Check(d.encoding_mode == L"eucjp", L"EUC-JP の文字コード設定を保存/復元できる");

    std::wstring ini2 = util::JoinPath(dir, L"config2.ini");
    FileManager::RemoveFile(ini2);
    ConfigManager cm2;
    cm2.SetPath(ini2);
    AppConfig e;
    cm2.Load(e);
    Check(e.search_similar_threshold == 70, L"類似一致の既定しきい値は 70%");
    Check(e.undo_max_generations == 10, L"Undo の既定世代数は 10");
    Check(e.search_line_from == 1 && e.search_line_to == 0, L"指定行範囲の既定値が正しい");
}

static void TestWarningDetection()
{
    Out(L"\n== 既存スイッチ警告 / include guard の除外 ==\n");
    std::wstring detail;

    // include guard だけのファイル
    std::wstring guarded =
        L"#ifndef SAMPLE_C\n"
        L"#define SAMPLE_C\n"
        L"\n"
        L"int a(void)\n"
        L"{\n"
        L"    return 0;\n"
        L"}\n"
        L"\n"
        L"int b(void)\n"
        L"{\n"
        L"    return 1;\n"
        L"}\n"
        L"\n"
        L"#endif /* SAMPLE_C */\n";
    Check(!SearchEngine::DetectNearbyPreprocessor(guarded, 5, 5, 5, detail),
          L"include guard の #ifndef 近傍では警告しない");
    Check(!SearchEngine::DetectNearbyPreprocessor(guarded, 12, 12, 5, detail),
          L"include guard の #endif 近傍では警告しない（誤検出の修正）");
    Check(detail.empty(), L"警告しないときは理由を残さない");

    // include guard + 内部に移行スイッチがあるファイル
    std::wstring inside =
        L"#ifndef SAMPLE_C\n"
        L"#define SAMPLE_C\n"
        L"\n"
        L"int a(void)\n"
        L"{\n"
        L"#ifdef LEGACY_MODE\n"
        L"    stock_alloc(item, qty);\n"
        L"#else\n"
        L"    stock_alloc(item, qty);\n"
        L"#endif\n"
        L"    return 0;\n"
        L"}\n"
        L"\n"
        L"#endif /* SAMPLE_C */\n";
    Check(SearchEngine::DetectNearbyPreprocessor(inside, 7, 7, 5, detail),
          L"既存スイッチの内側では警告する");
    Check(detail.find(L"内側") != std::wstring::npos, L"内側であることを理由に含める");
    Check(detail.find(L"LEGACY_MODE") != std::wstring::npos, L"検出したスイッチ名を含める");
    Check(detail.find(L"6 行目") != std::wstring::npos, L"検出した指令の行番号を含める");

    Check(SearchEngine::DetectNearbyPreprocessor(inside, 11, 11, 5, detail),
          L"ブロックの外でも近傍に条件指令があれば警告する");
    Check(detail.find(L"近傍") != std::wstring::npos, L"近傍であることを理由に含める");

    // 条件指令がまったく無いファイル
    Check(!SearchEngine::DetectNearbyPreprocessor(L"int a;\nint b;\nint c;\n", 2, 2, 5, detail),
          L"条件指令が無ければ警告しない");

    // 走査行数を 0 にすると近傍は検出しない（内側のみ）
    Check(!SearchEngine::DetectNearbyPreprocessor(inside, 11, 11, 0, detail),
          L"走査行数 0 では近傍を検出しない");
    Check(SearchEngine::DetectNearbyPreprocessor(inside, 7, 7, 0, detail),
          L"走査行数 0 でも内側は検出する");

    // #if !defined(NAME) 形式の include guard
    std::wstring guarded2 =
        L"#if !defined(SAMPLE2_C)\n"
        L"#define SAMPLE2_C\n"
        L"\n"
        L"int x(void)\n"
        L"{\n"
        L"    return 0;\n"
        L"}\n"
        L"\n"
        L"#endif\n";
    Check(!SearchEngine::DetectNearbyPreprocessor(guarded2, 5, 5, 5, detail),
          L"#if !defined() 形式の include guard も除外する");

    // include guard の外側にある本物のスイッチは検出する
    std::wstring guarded3 =
        L"#ifndef SAMPLE3_C\n"
        L"#define SAMPLE3_C\n"
        L"\n"
        L"#ifdef REAL_SWITCH\n"
        L"int a(void) { return 0; }\n"
        L"#endif\n"
        L"\n"
        L"#endif /* SAMPLE3_C */\n";
    Check(SearchEngine::DetectNearbyPreprocessor(guarded3, 5, 5, 5, detail),
          L"include guard と併存する本物のスイッチは検出する");
    Check(detail.find(L"REAL_SWITCH") != std::wstring::npos, L"本物のスイッチ名を含める");
}

static void TestAiLive(const std::wstring& keyfile)
{
    Out(L"\n== AI 連携（Genspark API 実疎通）==\n");
    if (keyfile.empty()) { Out(L"  [SKIP] キーファイルが指定されていません\n"); return; }
    std::string raw;
    ReadRaw(keyfile, raw);
    std::wstring key = util::Trim(util::Utf8ToWide(raw));
    if (key.empty()) { Out(L"  [SKIP] キーが空です\n"); return; }

    SpecInput in;
    in.system_name = L"在庫管理システム（検証用）";
    in.overview = L"C/S 構成の基幹システム。在庫引当処理を旧 API から新 API へ移行する。";
    in.purpose = L"旧 API のサポート終了に伴い新 API へ差し替える。";
    in.modification = L"stock_alloc() の呼び出しを new_stock_alloc() に差し替える。";
    in.switch_name = L"MIGRATION_001";
    in.old_code = L"stock_alloc(item, qty);";
    in.new_code = L"new_stock_alloc(item, qty, ctx);";
    in.target_summary = L"sample.c（1 ファイル / 1 箇所）";
    in.created_at = util::NowDateTime();

    AiSettings st;
    st.api_key = key;
    st.endpoint = AiClient::DefaultEndpoint();
    st.task_type = AiClient::DefaultTaskType();
    st.timeout_sec = 600;

    std::wstring prompt = SpecGenerator::BuildChangeSpecPrompt(in);
    std::wstring text, err;
    bool ok = AiClient::Ask(st, prompt, text, err);
    Check(ok, L"Genspark API へ接続し応答を取得する");
    if (!ok) { Out(L"  error: " + err + L"\n"); return; }

    Out(L"  response: " + std::to_wstring((int)text.size()) + L" chars\n");
    Check(text.size() > 200, L"変更仕様書として十分な分量の応答が返る");
    Check(text.find(L"#") != std::wstring::npos, L"応答が Markdown の見出しを含む");

    std::wstring html = SpecGenerator::WrapReportHtml(
        L"変更仕様書 - 在庫管理システム（検証用）", SpecGenerator::MarkdownToHtml(text), in);
    std::wstring path = util::JoinPath(VerifyDir(), L"live_changespec.html");
    std::wstring werr;
    Check(SpecGenerator::WriteReportFile(path, html, werr), L"実応答を HTML レポートへ書き出せる");
    Out(L"  report: " + path + L" (" + std::to_wstring((int)html.size()) + L" bytes)\n");

    std::wstring prompt2 = SpecGenerator::BuildInspectionSpecPrompt(in);
    std::wstring text2, err2;
    bool ok2 = AiClient::Ask(st, prompt2, text2, err2);
    Check(ok2, L"検査仕様書も Genspark API から取得できる");
    if (ok2) {
        std::wstring html2 = SpecGenerator::WrapReportHtml(
            L"検査仕様書 - 在庫管理システム（検証用）",
            SpecGenerator::MarkdownToHtml(text2), in);
        std::wstring path2 = util::JoinPath(VerifyDir(), L"live_inspectspec.html");
        Check(SpecGenerator::WriteReportFile(path2, html2, werr),
              L"検査仕様書を HTML レポートへ書き出せる");
        Out(L"  report2: " + path2 + L"\n");
    } else {
        Out(L"  error2: " + err2 + L"\n");
    }
}

// -----------------------------------------------------------------------------
// =============================================================================
// 追加機能のテスト
// =============================================================================
static void TestLogRuntime()
{
    Out(L"\n== ログ出力ランタイム ==\n");
    LogRuntimeOptions o;
    std::string h = LogRuntimeGenerator::HeaderText(o);
    Check(h.find("INPUT_LOG") != std::string::npos, L"ヘッダに INPUT_LOG がある");
    Check(h.find("OUTPUT_LOG") != std::string::npos, L"ヘッダに OUTPUT_LOG がある");
    Check(h.find("TEMP_LOG") != std::string::npos, L"ヘッダに TEMP_LOG がある");
    Check(h.find("ms_log_set_enabled") != std::string::npos, L"実行時 ON/OFF がある");
    Check(h.find("ms_log_count") != std::string::npos, L"件数カウンタがある");
    std::string c = LogRuntimeGenerator::SourceText(o);
    Check(c.find("ms_log_write") != std::string::npos, L"実装に書き出し関数がある");
    Check(c.find("MS_LOG_ENABLED") != std::string::npos, L"環境変数で無効化できる");

    std::wstring dir = util::JoinPath(VerifyDir(), L"logrt");
    std::vector<std::wstring> files; std::wstring err;
    Check(LogRuntimeGenerator::WriteTo(dir, o, files, err) && files.size() == 2,
          L"ヘッダと実装を書き出せる");
}

static void TestCodeLogger()
{
    Out(L"\n== ログ挿入 ==\n");
    std::wstring text =
        L"#include <stdio.h>\n\n"
        L"int add(int a, int b)\n{\n    int rc = a + b;\n    return rc;\n}\n";
    MatchResult m;
    m.file_path = L"x.c";
    m.start_offset = text.find(L"a + b");
    m.start_line = 5;
    m.selected = true;
    std::vector<MatchResult> sel; sel.push_back(m);

    LogInsertOptions o;
    o.header_name = L"ms_migration_log.h";
    o.switch_name = L"SW";
    LogInsertResult r; std::wstring err;
    Check(CodeLogger::Apply(text, sel, o, r, err), L"ログ挿入が成功する");
    Check(r.input_logs == 1 && r.output_logs == 1, L"INPUT 1 件 / OUTPUT 1 件を挿入する");
    Check(text.find(L"INPUT_LOG(\"add: enter\")") != std::wstring::npos, L"INPUT_LOG が入る");
    Check(text.find(L"OUTPUT_LOG(") != std::wstring::npos, L"OUTPUT_LOG が入る");
    Check(text.find(L"#include \"ms_migration_log.h\"") != std::wstring::npos, L"include が追加される");

    MatchResult m2 = m;
    m2.start_offset = text.find(L"a + b");
    std::vector<MatchResult> sel2; sel2.push_back(m2);
    LogInsertResult r2;
    CodeLogger::Apply(text, sel2, o, r2, err);
    size_t first = text.find(L"INPUT_LOG(\"add: enter\")");
    Check(first != std::wstring::npos && first == text.rfind(L"INPUT_LOG(\"add: enter\")"),
          L"再適用しても重複しない");
}

static void TestScopeReport()
{
    Out(L"\n== レポート作成単位 ==\n");
    ScopeEntry e;
    e.file_path = L"D:\\work\\order_alloc.c";
    e.encoding_name = L"Shift_JIS";
    e.newline_name = L"CRLF";
    e.before_text = L"a\nb\n";
    e.after_text = L"a\nB\n";
    MatchResult m;
    m.file_path = e.file_path;
    m.start_line = 2;
    m.matched_text = L"stock_alloc(item, qty);";
    m.matched_text = L"new_stock_alloc(item, qty, 0);";
    m.selected = true;
    e.matches.push_back(m);

    std::vector<ScopeEntry> entries; entries.push_back(e);
    std::wstring md = ScopeReportGenerator::BuildMarkdown(
        L"在庫管理", L"MIGRATION_001", L"2026-10-19 09:00:00", ReportScope::File, entries);
    Check(md.find(L"ファイル単位レポート") != std::wstring::npos, L"作成単位がファイル単位になる");
    Check(md.find(L"order_alloc.c") != std::wstring::npos, L"ファイル名を含む");
    Check(md.find(L"ファイル別サマリ") != std::wstring::npos, L"サマリ表がある");
    Check(md.find(L"new_stock_alloc") != std::wstring::npos, L"置換内容を含む");

    std::wstring html = ScopeReportGenerator::BuildHtml(L"t", md);
    Check(html.find(L"<!DOCTYPE html>") != std::wstring::npos, L"HTML を生成できる");
    Check(std::wstring(ScopeReportGenerator::ScopeLabel(ReportScope::Project)) == L"プロジェクト単位",
          L"単位名を返す");
}

static void TestGTestSignature()
{
    Out(L"\n== GoogleTest 生成 ==\n");
    std::wstring src =
        L"int order_reserve(int item, int qty)\n{\n    return 0;\n}\n\n"
        L"void order_reset(void)\n{\n}\n";
    GTestTarget t;
    Check(GTestGenerator::ParseSignature(src, L"order_reserve", t), L"関数シグネチャを解析できる");
    Check(t.return_type == L"int", L"戻り値型を取り出す");
    Check(t.param_names.size() == 2 && t.param_names[0] == L"item", L"引数名を取り出す");
    Check(t.callable, L"呼び出し可能と判定する");

    GTestTarget t2;
    Check(GTestGenerator::ParseSignature(src, L"order_reset", t2) && t2.param_decls.empty(),
          L"引数なし関数を解析できる");

    // 参照引数は対象外
    std::wstring src2 = L"int f(int& r)\n{\n    return r;\n}\n";
    GTestTarget t3;
    GTestGenerator::ParseSignature(src2, L"f", t3);
    Check(!t3.callable, L"参照引数は対象外にする");
}

int main(int argc, char** argv)
{
    TestLogRuntime();
    TestCodeLogger();
    TestScopeReport();
    TestGTestSignature();

    std::wstring a1 = (argc > 1) ? util::Utf8ToWide(argv[1]) : std::wstring();
    std::wstring a2 = (argc > 2) ? util::Utf8ToWide(argv[2]) : std::wstring();
    bool live = (a1 == L"--live");
    std::wstring keyfile = a2;
    ::SetConsoleOutputCP(CP_UTF8);
    Out(L"=====================================================\n");
    Out(L" CopyMigration Switcher - 単体テスト（設計書 20.1）\n");
    Out(L"=====================================================\n");

    TestSwitchName();
    TestSearch();
    TestGenerate();
    TestAutoIndent();
    TestPreviewAndReplace();
    TestEncodingAndNewline();
    TestBackupUndo();
    TestReport();
    TestSafety();
    TestInputChecks();

    // Phase 4: 17章 複数ファイル一括処理
    TestFolderScan();
    TestBatchSearch();
    TestBatchDryRunAndExecute();
    TestBatchSafety();
    TestSwitchHeaderManager();
    TestSwitchHeaderIntegration();
    TestSwitchHeaderUndo();
    TestAiClientBasics();
    TestSpecGenerator();
    TestAiConfigRoundtrip();
    TestAiOpenAiCompat();
    TestWarningDetection();
    TestCodeScope();
    TestSearchOptions();
    TestEucJp();
    TestMultiGenerationUndo();
    TestReportFormats();
    TestResultExporter();
    TestConfigRoundtripExt();
    if (live) TestAiLive(keyfile);

    Out(L"\n=====================================================\n");
    wchar_t buf[128];
    swprintf(buf, 128, L" 結果: PASS %d / FAIL %d\n", g_pass, g_fail);
    Out(buf);
    Out(L"=====================================================\n");
    return g_fail == 0 ? 0 : 1;
}
#else
#pragma message("console_test.cpp: MS_BUILD_CONSOLE_TESTS が未定義のため、"
                "この翻訳単位は空になります (GUI プロジェクトへ追加しても安全です)")
#endif

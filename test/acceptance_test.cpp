// =============================================================================
// acceptance_test.cpp
// 「今までの機能を一通り確認する」受け入れテスト（別 AI / 実機で実行する用）
//
//   make acceptance            … ビルド
//   build\acceptance_test.exe  … 実行（終了コード 0 = 全 PASS）
//
// GUI の見た目・操作は自動化できないため、docs/ACCEPTANCE_TEST.md の
// 「手動確認チェックリスト」と併用する。
// =============================================================================
#ifdef MS_BUILD_ACCEPTANCE_TESTS

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Models.h"
#include "Utils.h"
#include "FileManager.h"
#include "EncodingDetector.h"
#include "SearchEngine.h"
#include "CodeScope.h"
#include "SwitchGenerator.h"
#include "PreviewGenerator.h"
#include "DiffEngine.h"
#include "DiffReportGenerator.h"
#include "ValidationManager.h"
#include "BackupManager.h"
#include "UndoManager.h"
#include "FolderScanner.h"
#include "SwitchHeaderManager.h"
#include "ConfigManager.h"
#include "SpecGenerator.h"
#include "AiClient.h"
#include "IndentFormatter.h"
#include "HistoryManager.h"
#include "ScopeReportGenerator.h"
#include "GTestGenerator.h"
#include "LogRuntimeGenerator.h"

using namespace ms;

static int g_pass = 0;
static int g_fail = 0;
static std::vector<std::wstring> g_failed;

static void Out(const std::wstring& w)
{
    std::string u = util::WideToUtf8(w);
    fputs(u.c_str(), stdout);
    fflush(stdout);
}

static void Section(const wchar_t* name)
{
    Out(L"\n--- " + std::wstring(name) + L" ---\n");
}

static void Check(bool cond, const std::wstring& name)
{
    if (cond) { ++g_pass; Out(L"  [PASS] " + name + L"\n"); }
    else { ++g_fail; g_failed.push_back(name); Out(L"  [FAIL] " + name + L"\n"); }
}

static bool Contains(const std::wstring& hay, const std::wstring& needle)
{
    return hay.find(needle) != std::wstring::npos;
}

static std::wstring ReadAll(const std::wstring& path)
{
    std::wstring t;
    FileInfo fi;
    FileManager::ReadText(path, L"auto", t, fi);
    return t;
}

static std::wstring TempRoot()
{
    std::wstring d = util::JoinPath(util::ModuleDirectory(), L"acc_tmp");
    util::EnsureDirectory(d);
    return d;
}

static std::wstring NewDir(const std::wstring& name)
{
    std::wstring d = util::JoinPath(TempRoot(), name);
    util::EnsureDirectory(d);
    return d;
}

static void WriteFile(const std::wstring& path, const std::wstring& text)
{
    std::wstring err;
    FileManager::WriteTextAtomic(path, text, TextEncoding::UTF8, false,
                                 NewlineKind::LF, err);
}

// -----------------------------------------------------------------------------
static void TestUtilsAndFiles()
{
    Section(L"Utils / FileManager");
    Check(util::JoinPath(L"C:\\a", L"b.txt") == L"C:\\a\\b.txt", L"JoinPath");
    Check(util::GetFileNameOf(L"C:\\a\\b.txt") == L"b.txt", L"GetFileNameOf");

    std::vector<std::wstring> lines = util::SplitLines(L"a\nb\nc");
    Check(lines.size() == 3 && lines[2] == L"c", L"SplitLines");

    std::wstring crlf = util::NormalizeNewlines(L"a\r\nb\rc");
    Check(crlf == L"a\nb\nc", L"NormalizeNewlines");

    Check(util::ReplaceAll(L"x-y-x", L"x", L"z") == L"z-y-z", L"ReplaceAll");

    std::wstring jp = L"日本語テスト";
    Check(util::Utf8ToWide(util::WideToUtf8(jp)) == jp, L"UTF-8 変換 往復（日本語）");

    std::wstring dir = NewDir(L"utils");
    std::wstring f = util::JoinPath(dir, L"utf8.txt");
    WriteFile(f, L"一行目\n二行目\n");
    Check(util::PathExists(f), L"WriteTextAtomic（UTF-8）");

    std::wstring back; FileInfo fi;
    bool ok = FileManager::ReadText(f, L"auto", back, fi);
    Check(ok && back == L"一行目\n二行目\n", L"ReadText 往復（日本語）");
    Check(fi.encoding == TextEncoding::UTF8, L"文字コード判定 = UTF-8");

    std::wstring sj = util::JoinPath(dir, L"sjis.txt");
    std::wstring err;
    FileManager::WriteTextAtomic(sj, L"シフトJISのテスト\n", TextEncoding::ShiftJIS,
                                 false, NewlineKind::CRLF, err);
    FileInfo sjf;
    std::wstring sjt;
    ok = FileManager::ReadText(sj, L"auto", sjt, sjf);
    Check(ok && sjt == L"シフトJISのテスト\n" && sjf.encoding == TextEncoding::ShiftJIS,
          L"Shift_JIS 往復");
}

// -----------------------------------------------------------------------------
static std::vector<MatchResult> SelectAll(std::vector<MatchResult> v)
{
    for (size_t i = 0; i < v.size(); ++i) v[i].selected = true;
    return v;
}

static const wchar_t* kSample =
    L"int order_reserve(int id, int qty)\n"
    L"{\n"
    L"    int rc = 0;\n"
    L"    rc = old_alloc(id, qty);\n"
    L"    return rc;\n"
    L"}\n"
    L"\n"
    L"void logger_flush(void)\n"
    L"{\n"
    L"    old_alloc(0, 0);\n"
    L"}\n";

static void TestSearch()
{
    std::wstring ferr;
    Section(L"検索エンジン");
    std::vector<MatchResult> m = SelectAll(SearchEngine::Search(kSample, L"old_alloc", L"sample.c"));
    Check(m.size() == 2, L"完全一致: 2 件");
    Check(!m.empty() && m[0].start_line == 4, L"一致行番号 = 4");
    Check(!m.empty() && m[0].matched_text == L"old_alloc", L"一致文字列");
    Check(!m.empty() && m[0].start_column == 10, L"一致カラム = 10");

    std::vector<MatchResult> sim =
        SearchEngine::SearchSimilar(kSample, L"old_alloc ( id , qty )", L"sample.c", 50);
    Check(!sim.empty(), L"類似一致: 空白差を許容");
    if (!sim.empty()) Check(sim[0].match_rate < 100, L"類似一致: 一致率が 100 未満");

    std::vector<MatchResult> rng =
        SearchEngine::SearchInRange(kSample, L"old_alloc", L"sample.c", 1, 5);
    Check(rng.size() == 1, L"指定行範囲: 1〜5 行で 1 件");

    std::vector<MatchResult> fn =
        SearchEngine::SearchInFunction(kSample, L"old_alloc", L"sample.c", L"order_reserve", ferr);
    Check(fn.size() == 1, L"関数単位: order_reserve 内で 1 件");

    std::wstring detail;
    bool has_near = SearchEngine::DetectNearbyPreprocessor(
        L"#ifdef OLD_API\nint a = old_alloc(1,2);\n#endif\n", 2, 2, 5, detail);
    Check(has_near, L"既存スイッチ近傍を検出");

    bool guard = SearchEngine::DetectNearbyPreprocessor(
        L"#ifndef MY_H\n#define MY_H\nint a = old_alloc(1,2);\n#endif\n", 3, 3, 5, detail);
    Check(!guard, L"include guard は誤検出しない");

    bool guard2 = SearchEngine::DetectNearbyPreprocessor(
        L"#if !defined(MY_H)\n#define MY_H\nint a;\n#endif\n", 3, 3, 5, detail);
    Check(!guard2, L"#if !defined 形式の guard も誤検出しない");

    Check(SearchEngine::IsInsideComment(L"/* old_alloc */\n", 3), L"コメント内の一致を判定");
    Check(!SearchEngine::IsInsideComment(L"int old_alloc;\n", 5), L"コメント外は false");

    std::wstring rep = SearchEngine::ApplyReplacements(
        kSample, m, L"new_alloc(id, qty)");
    Check(Contains(rep, L"new_alloc(id, qty)"), L"置換の適用");
    Check(!Contains(rep, L"old_alloc"), L"置換後に旧コードが残らない");
}

// -----------------------------------------------------------------------------
static void TestSwitchAndIndent()
{
    Section(L"スイッチ生成 / インデント整形");
    std::vector<std::wstring> w;
    std::wstring a = SwitchGenerator::GenerateEx(
        SwitchStyle::IfdefNewFirst, L"MIG_001", L"old();", L"new();",
        false, L"", L"    ", w);
    Check(Contains(a, L"#ifdef MIG_001") && Contains(a, L"#else") && Contains(a, L"#endif"),
          L"方式A: #ifdef / #else / #endif");
    Check(Contains(a, L"new();") && a.find(L"new();") < a.find(L"old();"),
          L"方式A: 新コードが先");

    std::wstring b = SwitchGenerator::GenerateEx(
        SwitchStyle::IfndefOldFirst, L"MIG_001", L"old();", L"new();", false, L"", L"", w);
    Check(Contains(b, L"#ifndef MIG_001") && b.find(L"old();") < b.find(L"new();"),
          L"方式B: #ifndef / 旧コードが先");

    std::wstring c = SwitchGenerator::GenerateEx(
        SwitchStyle::IfdefWithComments, L"MIG_001", L"old();", L"new();", false, L"", L"", w);
    Check(Contains(c, L"migration start") && Contains(c, L"original"), L"方式C: コメント付き");

    std::wstring d = SwitchGenerator::GenerateEx(
        SwitchStyle::BlockCommentStyle, L"MIG_001", L"old();", L"new();", false, L"", L"", w);
    Check(Contains(d, L"/* MIG_001 start */") && Contains(d, L"/* MIG_001 end */"),
          L"方式D: 開始 / 終了コメント");

    std::wstring terr;
    Check(!SwitchGenerator::ValidateTemplate(L"${SWITCH}", terr),
          L"テンプレート検証: NEW_CODE 無しは不正");
    Check(SwitchGenerator::ValidateTemplate(
              L"#ifdef ${SWITCH}\n${NEW_CODE}\n#else\n${OLD_CODE}\n#endif", terr),
          L"テンプレート検証: 正しいテンプレート");

    std::wstring tpl = SwitchGenerator::GenerateEx(
        SwitchStyle::CustomTemplate, L"MIG_9", L"old();", L"new();", false,
        L"/* ${SWITCH} */\n${NEW_CODE}\n${OLD_CODE}", L"", w);
    Check(Contains(tpl, L"/* MIG_9 */") && Contains(tpl, L"new();"), L"テンプレート展開");

    std::wstring nested = L"if (a) {\n    if (b) {\n        x = 1;\n    }\n}";
    std::wstring ind = IndentFormatter::FormatBlock(nested, L"        ");
    Check(Contains(ind, L"        if (a) {"), L"インデント: 先頭行に基準インデント");
    Check(Contains(ind, L"            if (b) {"), L"インデント: ネスト 1 段 +4");
    Check(Contains(ind, L"                x = 1;"), L"インデント: ネスト 2 段 +8");

    std::wstring ded = IndentFormatter::Dedent(L"    a();\n        b();\n");
    Check(ded == L"a();\n    b();\n", L"共通インデントの除去");

    std::vector<std::wstring> aw;
    std::wstring ai = SwitchGenerator::ApplyAutoIndent(L"old();", L"new();", aw, L"    ");
    Check(Contains(ai, L"    new();"), L"ApplyAutoIndent（ヒント適用）");
}

// -----------------------------------------------------------------------------
static void TestPreviewAndDiff()
{
    Section(L"プレビュー / 差分");
    std::vector<MatchResult> m = SelectAll(SearchEngine::Search(kSample, L"old_alloc", L"sample.c"));
    std::vector<std::wstring> blocks;
    for (size_t i = 0; i < m.size(); ++i)
        blocks.push_back(L"#ifdef MIG\nnew_alloc(id, qty);\n#else\nold_alloc\n#endif");

    std::wstring after = PreviewGenerator::BuildAfterTextEach(kSample, m, blocks);
    Check(Contains(after, L"#ifdef MIG"), L"置換後テキストの生成");
    Check(Contains(after, L"new_alloc(id, qty)"), L"置換後テキストに新コード");

    std::wstring be, ae;
    bool ok = PreviewGenerator::BuildExcerpts(kSample, after, m, 2, be, ae);
    Check(ok && !be.empty() && !ae.empty(), L"抜粋の生成");

    std::wstring sbs = PreviewGenerator::BuildSideBySide(be, ae);
    Check(!sbs.empty(), L"左右比較表示");
    std::wstring tb = PreviewGenerator::BuildTopBottom(be, ae);
    Check(!tb.empty(), L"上下比較表示");
    std::wstring simple = PreviewGenerator::BuildSimple(ae);
    Check(!simple.empty(), L"単純表示");

    std::vector<std::wstring> lb = util::SplitLines(L"a\nb\nc\n");
    std::vector<std::wstring> la = util::SplitLines(L"a\nB\nc\nd\n");
    std::vector<DiffLine> dl;
    DiffEngine::ComputeLineDiff(lb, la, dl);
    Check(dl.size() >= 3, L"行差分: 行数が算出される");
    Check(wcslen(DiffEngine::ClassName(dl.empty() ? DIFF_NORMAL : dl[0].type)) > 0,
          L"行差分: 種別名");
    Check(!DiffEngine::RenderRows(dl).empty(), L"行差分の HTML 化");
}

// -----------------------------------------------------------------------------
static void TestReports()
{
    Section(L"レポート出力");
    std::wstring dir = NewDir(L"reports");
    OperationContext ctx;
    ctx.switch_name = L"MIG_001";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.old_code = L"old_alloc(id, qty);";
    ctx.new_code = L"new_alloc(id, qty);";
    ctx.operation_id = L"ACC-1";
    ctx.execution_mode = ExecutionMode::DryRun;
    ctx.encoding = TextEncoding::UTF8;
    ctx.newline = NewlineKind::LF;

    std::vector<MatchResult> m = SelectAll(SearchEngine::Search(kSample, L"old_alloc", L"sample.c"));
    ReplaceResult r;
    r.operation_id = ctx.operation_id;
    r.target_file_path = L"sample.c";
    r.before_text = kSample;
    r.after_text = PreviewGenerator::BuildAfterTextEach(
        kSample, m, std::vector<std::wstring>(m.size(), L"new_alloc(id, qty);"));
    r.replaced_count = (int)m.size();
    r.executed_at = util::NowDateTime();
    r.status = L"Success";

    std::wstring err;
    std::wstring html = util::JoinPath(dir, L"report.html");
    bool ok = DiffReportGenerator::Generate(html, ctx, m, kSample, r.after_text, r, err);
    Check(ok && util::PathExists(html), L"HTML 差分レポートの生成");
    std::wstring h = ReadAll(html);
    Check(Contains(h, L"MIG_001") && Contains(h, L"<html"), L"HTML レポートの内容");

    std::wstring md = util::JoinPath(dir, L"report.md");
    Check(DiffReportGenerator::GenerateMarkdown(md, ctx, m, kSample, r.after_text, r, err) &&
          util::PathExists(md), L"Markdown レポートの生成");
    std::wstring js = util::JoinPath(dir, L"report.json");
    Check(DiffReportGenerator::GenerateJson(js, ctx, m, kSample, r.after_text, r, err) &&
          util::PathExists(js), L"JSON レポートの生成");
    std::wstring j = ReadAll(js);
    Check(Contains(j, L"\"") && Contains(j, L"sample.c"), L"JSON レポートの内容");

    // スコープレポート（ファイル単位）— 今回の修正対象
    std::vector<ScopeEntry> entries;
    ScopeEntry e;
    e.file_path = L"C:\\work\\sample.c";
    e.encoding_name = L"UTF-8";
    e.newline_name = L"LF";
    e.before_text = kSample;
    e.after_text = r.after_text;
    e.status = L"Success";
    e.matches = m;
    entries.push_back(e);

    std::wstring smd = ScopeReportGenerator::BuildMarkdown(
        L"受注システム", L"MIG_001", L"2026-10-19 21:00:00", ReportScope::File, entries);
    Check(Contains(smd, L"ファイル単位レポート"), L"スコープ Markdown: 見出し");
    Check(Contains(smd, L"ファイル別サマリ"), L"スコープ Markdown: サマリ表");

    std::wstring shtml = ScopeReportGenerator::BuildHtmlFromEntries(
        L"受注システム", L"MIG_001", L"2026-10-19 21:00:00", ReportScope::File, entries);
    Check(Contains(shtml, L"<!DOCTYPE html>"), L"スコープ HTML: DOCTYPE");
    Check(Contains(shtml, L"<table>") && Contains(shtml, L"</table>"), L"スコープ HTML: 表");
    Check(Contains(shtml, L"<details>"), L"スコープ HTML: 差分の折りたたみ");
    Check(!Contains(shtml, L"| ---"), L"スコープ HTML: Markdown 表が残らない");
    Check(Contains(shtml, L"&lt;") || !Contains(shtml, L"<script"), L"スコープ HTML: エスケープ");
}

// -----------------------------------------------------------------------------
static void TestValidationAndBackup()
{
    Section(L"入力検証 / バックアップ / Undo");
    Check(ValidationManager::ValidateSwitchName(L"MIG_001").ok, L"スイッチ名: 正常");
    Check(!ValidationManager::ValidateSwitchName(L"1MIG").ok, L"スイッチ名: 数字始まりはエラー");
    Check(!ValidationManager::ValidateSwitchName(L"#define X").ok, L"スイッチ名: 記号入りはエラー");
    Check(ValidationManager::IsReservedWord(L"int"), L"予約語の判定");
    Check(!ValidationManager::ValidateOldCode(L"").ok, L"旧コード: 空はエラー");
    Check(ValidationManager::ValidateNewCode(L"old();", L"new();").ok, L"新コード: 正常");

    std::wstring dir = NewDir(L"backup");
    std::wstring f = util::JoinPath(dir, L"target.c");
    WriteFile(f, L"int a = 1;\n");
    std::wstring bp, err;
    bool ok = BackupManager::CreateBackup(f, dir, bp, err);
    Check(ok && util::PathExists(bp), L"バックアップ作成");
    Check(bp.size() > 4 && bp.substr(bp.size() - 4) == L".bak", L"バックアップ名 = *.bak");
    Check(Contains(bp, L"target.c"), L"バックアップ名に元ファイル名");
    Check(ReadAll(bp) == L"int a = 1;\n", L"バックアップ内容が一致");

    UndoManager um;
    um.SetMaxGenerations(5);
    UndoRecord rec;
    rec.target_file_path = f;
    rec.backup_file_path = bp;
    rec.operation_id = L"OP-1";
    rec.executed_at = util::NowDateTime();
    rec.valid = true;
    um.SetRecord(rec);
    Check(um.CanUndo() && um.HistorySize() == 1, L"Undo: 1 世代を記録");

    WriteFile(f, L"int a = 2;\n");
    std::vector<std::wstring> side;
    ok = um.Undo(err, side);
    Check(ok, L"Undo 実行");
    Check(ReadAll(f) == L"int a = 1;\n", L"Undo で内容が戻る");
    Check(um.HistorySize() == 0, L"Undo 後に世代が減る");

    // 複数世代
    UndoManager um2;
    for (int i = 0; i < 3; ++i) {
        WriteFile(f, L"v" + std::to_wstring(i) + L"\n");
        std::wstring b2;
        BackupManager::CreateBackup(f, dir, b2, err);
        UndoRecord r2;
        r2.target_file_path = f;
        r2.backup_file_path = b2;
        r2.operation_id = L"OP-" + std::to_wstring(i);
        r2.executed_at = util::NowDateTime();
        r2.valid = true;
        um2.SetRecord(r2);
    }
    Check(um2.HistorySize() == 3, L"複数世代 Undo: 3 世代");
    Check(!um2.GenerationLabel(0).empty(), L"世代ラベルの生成");
}

// -----------------------------------------------------------------------------
static void TestScannerAndConfig()
{
    Section(L"フォルダ走査 / 設定ファイル");
    std::wstring dir = NewDir(L"scan");
    WriteFile(util::JoinPath(dir, L"a.c"), L"old_alloc(1,2);\n");
    WriteFile(util::JoinPath(dir, L"b.cpp"), L"old_alloc(3,4);\n");
    WriteFile(util::JoinPath(dir, L"c.txt"), L"old_alloc\n");
    std::wstring sub = util::JoinPath(dir, L"sub");
    util::EnsureDirectory(sub);
    WriteFile(util::JoinPath(sub, L"d.c"), L"old_alloc(5,6);\n");

    std::vector<std::wstring> exts;
    exts.push_back(L".c");
    exts.push_back(L".cpp");

    ScanResult r1;
    FolderScanner::Scan(dir, exts, false, r1);
    Check(r1.files.size() == 2, L"走査（サブフォルダ無し）: 2 件");
    ScanResult r2;
    FolderScanner::Scan(dir, exts, true, r2);
    Check(r2.files.size() == 3, L"走査（サブフォルダ有り）: 3 件");
    Check(FolderScanner::MatchesExtension(L"x.C", exts), L"拡張子判定（大文字）");

    std::wstring ini = util::JoinPath(NewDir(L"cfg"), L"config_test.ini");
    ConfigManager cm;
    cm.SetPath(ini);
    AppConfig c;
    c.switch_memo = L"MIG_777";
    c.auto_indent = true;
    c.create_backup = true;
    c.report_scope = ReportScope::Project;
    c.log_enabled = true;
    c.log_insert_temp = true;
    c.log_return_value = true;
    c.log_source_cpp = true;
    c.gtest_enabled = true;
    c.gtest_load_enabled = false;
    c.gtest_load_iterations = 250;
    c.winmerge_path = L"C:\\Tools\\WinMergeU.exe";
    cm.Save(c);
    cm.SaveExtras(c);

    ConfigManager cm2;
    cm2.SetPath(ini);
    AppConfig d;
    cm2.Load(d);
    Check(d.switch_memo == L"MIG_777", L"設定: スイッチメモ 往復");
    Check(d.auto_indent && d.create_backup, L"設定: 真偽値 往復");
    Check(d.report_scope == ReportScope::Project, L"設定: レポート単位 往復");
    Check(d.log_enabled && d.log_insert_temp && d.log_return_value, L"設定: ログ設定 往復");
    Check(d.log_source_cpp, L"設定: ログ実装 .cpp 往復");
    Check(d.gtest_enabled && !d.gtest_load_enabled, L"設定: 負荷試験トグル 往復");
    Check(d.gtest_load_iterations == 250, L"設定: 負荷試験回数 往復");
    Check(d.winmerge_path == L"C:\\Tools\\WinMergeU.exe", L"設定: WinMerge パス 往復");
}

// -----------------------------------------------------------------------------
static void TestLogRuntime()
{
    Section(L"ログ出力ランタイム");
    std::wstring dir = NewDir(L"logrt");

    LogRuntimeOptions o;
    o.header_name = L"ms_migration_log.h";
    o.source_name = L"ms_migration_log.c";
    o.default_enabled = true;
    std::wstring hs = util::Utf8ToWide(LogRuntimeGenerator::HeaderText(o));
    std::wstring ss = util::Utf8ToWide(LogRuntimeGenerator::SourceText(o));

    Check(Contains(hs, L"INPUT_LOG") && Contains(hs, L"OUTPUT_LOG") && Contains(hs, L"TEMP_LOG"),
          L"ヘッダ: 3 種のマクロ");
    Check(Contains(hs, L"MS_LOG_KIND_INPUT") && Contains(hs, L"MS_LOG_KIND_OUTPUT") &&
          Contains(hs, L"MS_LOG_KIND_TEMP"), L"ヘッダ: 種別の定義");
    Check(Contains(hs, L"ms_log_set_enabled"), L"ヘッダ: 実行時 ON/OFF API");
    Check(Contains(hs, L"MS_LOG_ENABLED"), L"ヘッダ: 環境変数による無効化");

    // 実装側の単体コンパイル保証（報告されたエラーの再発防止）
    Check(Contains(ss, L"#ifndef MS_LOG_KIND_INPUT"), L"実装: 種別の予備定義");
    Check(Contains(ss, L"MS_LOG_LINKAGE"), L"実装: 宣言の自己完結");
    Check(Contains(ss, L"#ifndef MS_LOG_KIND_OUTPUT") &&
          Contains(ss, L"#ifndef MS_LOG_KIND_TEMP"), L"実装: 出力/一時の予備定義");
    Check(Contains(ss, L"case MS_LOG_KIND_INPUT"), L"実装: switch は定数マクロのみ");

    // コメント化け防止（ASCII のみ）
    bool ascii_h = true, ascii_s = true;
    for (size_t i = 0; i < hs.size(); ++i) if ((unsigned)hs[i] > 127) { ascii_h = false; break; }
    for (size_t i = 0; i < ss.size(); ++i) if ((unsigned)ss[i] > 127) { ascii_s = false; break; }
    Check(ascii_h, L"ヘッダは ASCII のみ（文字化けしない）");
    Check(ascii_s, L"実装は ASCII のみ（文字化けしない）");

    // .c / .cpp の切り替え
    LogRuntimeOptions oc = o;
    std::vector<std::wstring> files;
    std::wstring err;
    Check(LogRuntimeGenerator::WriteTo(dir, oc, files, err) && files.size() == 2,
          L".c として出力");
    LogRuntimeOptions opp = o;
    opp.source_is_cpp = true;
    std::vector<std::wstring> files2;
    Check(LogRuntimeGenerator::WriteTo(dir, opp, files2, err), L".cpp として出力");
    Check(files2.size() == 2 &&
          util::GetFileNameOf(files2[1]) == L"ms_migration_log.cpp", L".cpp のファイル名");
    Check(Contains(ReadAll(files2[1]), L"MS_LOG_LINKAGE"), L".cpp でも宣言が揃う");
}

// -----------------------------------------------------------------------------
static void TestHistory()
{
    Section(L"履歴（undobak）");
    std::wstring base = NewDir(L"hist_" + util::TimestampFileName());
    std::wstring b = util::JoinPath(base, L"before.c");
    std::wstring a = util::JoinPath(base, L"after.c");
    WriteFile(b, L"int a = 1;\n");
    WriteFile(a, L"int a = 2;\n");

    std::vector<HistoryFileEntry> src;
    HistoryFileEntry e;
    e.path = util::JoinPath(base, L"target.c");
    e.before = b; e.after = a;
    e.has_before = e.has_after = true;
    src.push_back(e);

    HistoryRunInfo run;
    std::wstring err;
    Check(HistoryManager::Record(base, L"OP-ACC", L"Real", 1, src, run, err),
          L"履歴の記録");
    Check(!run.id.empty(), L"実行 ID の生成");

    std::wstring root = HistoryManager::RootFor(base);
    Check(Contains(root, L"undobak"), L"undobak フォルダ配下に保存");
    Check(util::PathExists(util::JoinPath(base, L"undobak\\runs")), L"runs フォルダ");
    Check(Contains(HistoryManager::SidecarDirectory(base), L"undobak"), L"sidecars フォルダ");

    std::vector<HistoryRunInfo> runs = HistoryManager::ListRuns(base);
    bool found = false;
    for (size_t i = 0; i < runs.size(); ++i) if (runs[i].id == run.id) found = true;
    Check(runs.size() >= 1 && found, L"履歴一覧に今回の実行が含まれる");
    std::vector<HistoryFileEntry> list = HistoryManager::ListFiles(base, run.id);
    Check(list.size() == 1 && list[0].path == e.path, L"履歴ファイル一覧");
    Check(list.size() == 1 && list[0].has_before && list[0].has_after,
          L"変更前 / 変更後のスナップショット");

    std::wstring diff = HistoryManager::BuildDiffText(L"a\nb\n", L"a\nB\n", 100);
    Check(Contains(diff, L"- b") && Contains(diff, L"+ B"), L"履歴の差分テキスト");
    Check(!HistoryManager::RunLabel(run).empty(), L"履歴ラベル");
}

// -----------------------------------------------------------------------------
static void TestGTest()
{
    Section(L"GoogleTest 生成");
    GTestTarget t1;
    bool ok = GTestGenerator::ParseSignature(
        L"int order_reserve(int id, int qty)\n{\n    return 0;\n}\n",
        L"order_reserve", t1);
    Check(ok && t1.callable, L"シグネチャ解析: 引数 2 つ");
    Check(t1.param_decls.size() == 2, L"シグネチャ解析: 引数の数");
    Check(t1.return_type == L"int", L"シグネチャ解析: 戻り型");

    GTestTarget t2;
    GTestGenerator::ParseSignature(L"void flush(void)\n{\n}\n", L"flush", t2);
    Check(t2.callable, L"シグネチャ解析: void 関数");

    GTestTarget t3;
    GTestGenerator::ParseSignature(
        L"int sum(int n, ...)\n{\n    return n;\n}\n", L"sum", t3);
    Check(!t3.callable, L"可変長引数は DISABLED 扱い");

    GTestTarget t4;
    GTestGenerator::ParseSignature(
        L"void fill(int& out)\n{\n}\n", L"fill", t4);
    Check(!t4.callable, L"参照引数は DISABLED 扱い");

    std::vector<MatchResult> m = SelectAll(SearchEngine::Search(kSample, L"old_alloc", L"sample.c"));
    std::vector<GTestTarget> targets =
        GTestGenerator::CollectTargets(kSample, L"sample.c", m);
    Check(targets.size() == 2, L"対象関数の抽出（重複除去）");

    std::wstring dir = NewDir(L"gtest");
    GTestOptions o;
    o.out_dir = dir;
    o.load_iterations = 100;
    o.generate_load = true;
    o.switch_name = L"MIG_001";
    std::vector<std::wstring> files;
    std::wstring err;
    Check(GTestGenerator::WriteProject(dir, targets, o, files, err), L"テストプロジェクト生成");
    Check(util::PathExists(util::JoinPath(dir, L"generated_tests.cpp")), L"generated_tests.cpp");
    Check(util::PathExists(util::JoinPath(dir, L"CMakeLists.txt")), L"CMakeLists.txt");

    std::wstring gen = ReadAll(util::JoinPath(dir, L"generated_tests.cpp"));
    Check(Contains(gen, L"TEST("), L"テストコード: TEST マクロ");
    Check(Contains(gen, L"MS_LOAD_ITERATIONS"), L"テストコード: 負荷試験の反復");
    Check(Contains(gen, L"GTEST_SKIP"), L"テストコード: 0 でスキップ");

    std::wstring cm = ReadAll(util::JoinPath(dir, L"CMakeLists.txt"));
    Check(Contains(cm, L"MS_SWITCH_ON"), L"CMake: スイッチ外部切替");
    Check(Contains(cm, L"add_compile_definitions(MIG_001)"), L"CMake: スイッチ名の定義");
    Check(Contains(cm, L"GTest") || Contains(cm, L"googletest"), L"CMake: GoogleTest 取得");
}

// -----------------------------------------------------------------------------
static void TestSpecAndAi()
{
    Section(L"仕様書 / AI 連携");
    SpecInput in;
    in.system_name = L"受注システム";
    in.overview = L"受注を管理する";
    in.purpose = L"API 移行";
    in.modification = L"alloc 差し替え";
    in.switch_name = L"MIG_001";
    in.old_code = L"old_alloc(1,2);";
    in.new_code = L"new_alloc(1,2);";
    in.created_at = L"2026-10-19";

    std::wstring p = SpecGenerator::BuildChangeSpecPrompt(in);
    Check(Contains(p, L"受注システム"), L"プロンプト: システム名");
    Check(Contains(p, L"MIG_001"), L"プロンプト: スイッチ名");
    Check(Contains(p, L"old_alloc") && Contains(p, L"new_alloc"), L"プロンプト: 旧/新コード");

    std::wstring html = SpecGenerator::MarkdownToHtml(L"# 見出し\n\n**太字** と `code`\n");
    Check(Contains(html, L"<h1>") && Contains(html, L"<strong>"), L"Markdown → HTML 変換");
    Check(Contains(SpecGenerator::EscapeHtml(L"<a&b>"), L"&lt;"), L"HTML エスケープ");
    Check(SpecGenerator::SanitizeFileName(L"a:b*c?.md").find(L':') == std::wstring::npos,
          L"ファイル名のサニタイズ");

    Check(Contains(AiClient::JsonEscape(L"a\"b\nc"), L"\\\""), L"JSON エスケープ");
    Check(!AiClient::DefaultEndpoint().empty(), L"既定エンドポイント");
    Check(!AiClient::ResolveOpenAiUrl(L"https://api.openai.com/v1").empty(),
          L"OpenAI 互換 URL の解決");
}

// -----------------------------------------------------------------------------
static void TestEndToEnd()
{
    Section(L"エンドツーエンド（検索 → 置換 → 履歴 → 復元）");
    std::wstring dir = NewDir(L"e2e");
    std::wstring f = util::JoinPath(dir, L"order.c");
    WriteFile(f, kSample);

    std::wstring orig = ReadAll(f);
    std::vector<MatchResult> m = SelectAll(SearchEngine::Search(orig, L"old_alloc", f));
    Check(m.size() == 2, L"E2E: 検索");

    OperationContext ctx;
    ctx.switch_name = L"MIG_E2E";
    ctx.switch_style = SwitchStyle::IfdefNewFirst;
    ctx.old_code = L"old_alloc";
    ctx.new_code = L"new_alloc";
    ctx.auto_indent = true;

    std::vector<std::wstring> blocks;
    for (size_t i = 0; i < m.size(); ++i) {
        std::wstring hint = SearchEngine::IndentHintAt(orig, m[i]);
        std::wstring oc = IndentFormatter::FormatBlock(ctx.old_code, hint);
        std::wstring nc = IndentFormatter::FormatBlock(ctx.new_code, hint);
        std::vector<std::wstring> w;
        blocks.push_back(SwitchGenerator::GenerateEx(
            ctx.switch_style, ctx.switch_name, oc, nc, false, L"", hint, w));
    }
    std::wstring after = PreviewGenerator::BuildAfterTextEach(orig, m, blocks);
    Check(Contains(after, L"#ifdef MIG_E2E"), L"E2E: スイッチ生成");
    Check(Contains(after, L"    new_alloc"), L"E2E: インデントが既存コードに合う");

    std::wstring bak, err;
    Check(BackupManager::CreateBackup(f, util::JoinPath(dir, L"undobak\\sidecars"), bak, err),
          L"E2E: バックアップ");
    WriteFile(f, after);
    Check(ReadAll(f) == after, L"E2E: 書き込み");

    HistoryFileEntry he;
    he.path = f; he.before = bak; he.has_before = true;
    std::wstring snap = util::JoinPath(dir, L"after_snap.c");
    WriteFile(snap, after);
    he.after = snap; he.has_after = true;
    std::vector<HistoryFileEntry> v;
    v.push_back(he);
    HistoryRunInfo run;
    Check(HistoryManager::Record(dir, L"OP-E2E", L"Real", 2, v, run, err), L"E2E: 履歴記録");

    UndoManager um;
    UndoRecord rec;
    rec.target_file_path = f;
    rec.backup_file_path = bak;
    rec.executed_at = util::NowDateTime();
    rec.operation_id = L"OP-E2E";
    rec.valid = true;
    um.SetRecord(rec);
    std::vector<std::wstring> side;
    Check(um.Undo(err, side), L"E2E: Undo");
    Check(ReadAll(f) == orig, L"E2E: 元の内容へ完全復元");
}

// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    (void)argc; (void)argv;
    ::SetConsoleOutputCP(65001);
    Out(L"=====================================================\n");
    Out(L" CopyMigration Switcher 受け入れテスト\n");
    Out(L"=====================================================\n");

    TestUtilsAndFiles();
    TestSearch();
    TestSwitchAndIndent();
    TestPreviewAndDiff();
    TestReports();
    TestValidationAndBackup();
    TestScannerAndConfig();
    TestLogRuntime();
    TestHistory();
    TestGTest();
    TestSpecAndAi();
    TestEndToEnd();

    Out(L"\n=====================================================\n");
    wchar_t buf[160];
    swprintf(buf, 160, L" 結果: PASS %d / FAIL %d\n", g_pass, g_fail);
    Out(buf);
    if (!g_failed.empty()) {
        Out(L" 失敗した項目:\n");
        for (size_t i = 0; i < g_failed.size(); ++i)
            Out(L"   - " + g_failed[i] + L"\n");
    }
    Out(L"=====================================================\n");

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, util::JoinPath(util::ModuleDirectory(), L"acceptance_result.txt").c_str(),
                  L"w, ccs=UTF-8") == 0 && fp) {
        swprintf(buf, 160, L"PASS %d / FAIL %d\r\n", g_pass, g_fail);
        fputws(buf, fp);
        for (size_t i = 0; i < g_failed.size(); ++i) {
            fputws(L"FAIL: ", fp);
            fputws(g_failed[i].c_str(), fp);
            fputws(L"\r\n", fp);
        }
        fclose(fp);
    }
    return g_fail == 0 ? 0 : 1;
}

#else
#pragma message("acceptance_test.cpp: MS_BUILD_ACCEPTANCE_TESTS が未定義のため空です")
#endif

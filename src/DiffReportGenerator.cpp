// =============================================================================
// DiffReportGenerator.cpp
// 12.3 HTML レポート CSS 方針に従った色分けを実装する
// =============================================================================
#include "DiffReportGenerator.h"
#include "DiffEngine.h"
#include "EncodingDetector.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

namespace {

std::wstring CommonCss()
{
    std::wstring h;
    h += L"body{font-family:'Segoe UI','Yu Gothic UI',Meiryo,sans-serif;margin:24px;"
         L"background:#fafbfc;color:#1f2328;}\n";
    h += L"h1{font-size:22px;border-bottom:2px solid #2f81f7;padding-bottom:8px;}\n";
    h += L"h2{font-size:16px;margin-top:28px;color:#1f2328;}\n";
    h += L"h3{font-size:14px;margin-top:22px;padding:6px 10px;background:#eef2f6;"
         L"border-left:4px solid #2f81f7;border-radius:3px;"
         L"font-family:Consolas,'MS Gothic',monospace;word-break:break-all;}\n";
    h += L"table.meta{border-collapse:collapse;margin:12px 0 20px;background:#fff;"
         L"box-shadow:0 1px 3px rgba(0,0,0,.08);}\n";
    h += L"table.meta th,table.meta td{border:1px solid #d0d7de;padding:6px 12px;"
         L"font-size:13px;text-align:left;vertical-align:top;}\n";
    h += L"table.meta th{background:#f0f3f6;width:170px;font-weight:600;}\n";
    h += L"pre.code{background:#fff;border:1px solid #d0d7de;border-radius:6px;"
         L"padding:12px;overflow-x:auto;font-family:Consolas,'MS Gothic',monospace;"
         L"font-size:12.5px;line-height:1.5;}\n";
    h += L"table.diff{border-collapse:collapse;width:100%;background:#fff;"
         L"border:1px solid #d0d7de;font-family:Consolas,'MS Gothic',monospace;"
         L"font-size:12.5px;}\n";
    h += L"table.diff td{border:1px solid #e6e9ed;padding:2px 8px;white-space:pre-wrap;}\n";
    h += L"table.diff td.ln{width:64px;text-align:right;color:#6e7781;"
         L"background:#f6f8fa;user-select:none;}\n";
    // 12.3 HTML レポート CSS 方針
    h += L".added{background-color:#e6ffed;}\n";
    h += L".deleted{background-color:#ffeef0;}\n";
    h += L".changed{background-color:#fff5b1;}\n";
    h += L".warning{background-color:#ffd580;}\n";
    h += L".normal{background-color:#ffffff;}\n";
    h += L".badge{display:inline-block;padding:2px 10px;border-radius:10px;"
         L"font-size:12px;font-weight:600;}\n";
    h += L".badge.ok{background:#dafbe1;color:#1a7f37;}\n";
    h += L".badge.ng{background:#ffebe9;color:#cf222e;}\n";
    h += L".badge.warn{background:#fff8c5;color:#9a6700;}\n";
    h += L".badge.skip{background:#eaeef2;color:#57606a;}\n";
    h += L".warnlist li{background:#ffd580;padding:4px 8px;margin:4px 0;"
         L"border-radius:4px;font-size:13px;list-style:none;}\n";
    h += L".foot{margin-top:32px;color:#6e7781;font-size:11.5px;}\n";
    return h;
}

std::wstring WrapHtml(const std::wstring& title, const std::wstring& body)
{
    std::wstring h;
    h += L"<!DOCTYPE html>\n<html lang=\"ja\">\n<head>\n";
    h += L"<meta charset=\"utf-8\">\n";
    h += L"<title>" + title + L"</title>\n";
    h += L"<style>\n" + CommonCss() + L"</style>\n</head>\n<body>\n";
    h += body;
    h += L"</body>\n</html>\n";
    return h;
}

std::wstring FolderExtText(const std::vector<std::wstring>& exts)
{
    std::wstring out;
    for (size_t i = 0; i < exts.size(); ++i) {
        if (i) out += L" ";
        out += exts[i];
    }
    return out.empty() ? std::wstring(L"(既定)") : out;
}

std::wstring StatusBadge(const std::wstring& status)
{
    if (status == L"Success")  return L"<span class=\"badge ok\">成功</span>";
    if (status == L"Failed")   return L"<span class=\"badge ng\">失敗</span>";
    if (status == L"Warning")  return L"<span class=\"badge warn\">警告</span>";
    if (status == L"Skipped")  return L"<span class=\"badge skip\">スキップ</span>";
    if (status == L"DryRun")   return L"<span class=\"badge skip\">ドライラン</span>";
    if (status == L"対象外")   return L"<span class=\"badge skip\">対象外</span>";
    if (status.empty())        return L"<span class=\"badge skip\">未実行</span>";
    return L"<span class=\"badge warn\">" + util::EscapeHtml(status) + L"</span>";
}

std::wstring MatchTypeText(MatchType t)
{
    switch (t) {
    case MatchType::Normalized: return L"正規化一致";
    case MatchType::Similar:    return L"類似一致";
    default:                    return L"完全一致";
    }
}

std::wstring MatchStatusText(MatchStatus s)
{
    switch (s) {
    case MatchStatus::Excluded: return L"除外";
    case MatchStatus::Warning:  return L"警告";
    case MatchStatus::Error:    return L"エラー";
    default:                    return L"対象";
    }
}

// 対象箇所一覧の表
std::wstring MatchTable(const std::vector<MatchResult>& matches, bool with_file)
{
    std::wstring h;
    h += L"<table class=\"meta\">\n<tr>";
    if (with_file) h += L"<th>ファイル</th>";
    h += L"<th>No</th><th>開始行</th><th>終了行</th><th>開始列</th>"
         L"<th>一致種別</th><th>状態</th></tr>\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        const MatchResult& m = matches[i];
        h += L"<tr>";
        if (with_file)
            h += L"<td>" + util::EscapeHtml(util::GetFileNameOf(m.file_path)) + L"</td>";
        h += L"<td>" + std::to_wstring(m.match_id) + L"</td>";
        h += L"<td>" + std::to_wstring(m.start_line) + L"</td>";
        h += L"<td>" + std::to_wstring(m.end_line) + L"</td>";
        h += L"<td>" + std::to_wstring(m.start_column) + L"</td>";
        h += L"<td>" + MatchTypeText(m.match_type) + L"</td>";
        h += L"<td>" + MatchStatusText(m.status) + L"</td>";
        h += L"</tr>\n";
    }
    h += L"</table>\n";
    return h;
}

// 変更前・変更後・差分（1 ファイル分）
std::wstring FileDiffBody(const std::vector<MatchResult>& matches,
                          const std::wstring& before_text,
                          const std::wstring& after_text)
{
    int first_line = 1, last_line = 1;
    if (!matches.empty()) {
        first_line = matches[0].start_line;
        last_line = matches[0].end_line;
        for (size_t i = 1; i < matches.size(); ++i) {
            if (matches[i].start_line < first_line) first_line = matches[i].start_line;
            if (matches[i].end_line > last_line) last_line = matches[i].end_line;
        }
    }
    const int CTX = 20;
    std::vector<std::wstring> bl = util::SplitLines(before_text);
    std::vector<std::wstring> al = util::SplitLines(after_text);

    std::vector<std::wstring> b_ex, a_ex;
    int b_from = 1, a_from = 1;
    DiffEngine::MakeExcerpt(bl, al, first_line, last_line, CTX, b_ex, a_ex, b_from, a_from);

    std::vector<DiffLine> diff;
    DiffEngine::ComputeLineDiff(b_ex, a_ex, diff);

    std::wstring h;
    h += L"<h2>変更前</h2>\n<pre class=\"code\">";
    h += DiffEngine::PreWithLineNumbers(b_ex, b_from);
    h += L"</pre>\n";
    h += L"<h2>変更後</h2>\n<pre class=\"code\">";
    h += DiffEngine::PreWithLineNumbers(a_ex, a_from);
    h += L"</pre>\n";
    h += L"<h2>差分表示</h2>\n<table class=\"diff\">\n";
    h += DiffEngine::RenderRows(diff);
    h += L"</table>\n";
    return h;
}

std::wstring WarningList(const std::vector<MatchResult>& matches)
{
    bool any = false;
    std::wstring h;
    for (size_t i = 0; i < matches.size(); ++i) {
        for (size_t w = 0; w < matches[i].warning_flags.size(); ++w) {
            if (!any) { h += L"<h2>警告</h2>\n<ul class=\"warnlist\">\n"; any = true; }
            h += L"<li>No." + std::to_wstring(matches[i].match_id) + L" (行 " +
                 std::to_wstring(matches[i].start_line) + L"): " +
                 util::EscapeHtml(matches[i].warning_flags[w]) + L"</li>\n";
        }
    }
    if (any) h += L"</ul>\n";
    return h;
}

} // namespace

std::wstring DiffReportGenerator::BuildReportPath(const std::wstring& report_dir,
                                                  const std::wstring& target_file,
                                                  const std::wstring& stamp)
{
    std::wstring name = util::GetBaseNameNoExt(target_file);
    if (name.empty()) name = L"target";
    return util::JoinPath(report_dir, stamp + L"_" + name + L"_diff.html");
}

std::wstring DiffReportGenerator::BuildIntegratedReportPath(const std::wstring& report_dir,
                                                            const std::wstring& folder,
                                                            const std::wstring& stamp)
{
    std::wstring name = util::GetFileNameOf(folder);
    if (name.empty()) name = util::GetBaseNameNoExt(folder);
    if (name.empty()) name = L"batch";
    return util::JoinPath(report_dir, stamp + L"_" + name + L"_batch_diff.html");
}

// -----------------------------------------------------------------------------
// 6.12 単一ファイル版
// -----------------------------------------------------------------------------
std::wstring DiffReportGenerator::BuildHtml(const OperationContext& ctx,
                                            const std::vector<MatchResult>& matches,
                                            const std::wstring& before_text,
                                            const std::wstring& after_text,
                                            const ReplaceResult& result)
{
    std::wstring body;
    body += L"<h1>Migration Switch Report</h1>\n";

    body += L"<table class=\"meta\">\n";
    body += L"<tr><th>実行日時</th><td>" + util::EscapeHtml(ctx.started_at) + L"</td></tr>\n";
    body += L"<tr><th>対象ファイル</th><td>" + util::EscapeHtml(ctx.target_file_path) + L"</td></tr>\n";
    body += L"<tr><th>スイッチ名</th><td><code>" + util::EscapeHtml(ctx.switch_name) + L"</code></td></tr>\n";
    body += L"<tr><th>実行モード</th><td>" +
            std::wstring(ctx.execution_mode == ExecutionMode::DryRun
                         ? L"ドライラン（ファイル未変更）" : L"本実行") + L"</td></tr>\n";
    body += L"<tr><th>ヒット件数</th><td>" + std::to_wstring((int)matches.size()) + L" 件</td></tr>\n";
    body += L"<tr><th>置換件数</th><td>" + std::to_wstring(result.replaced_count) + L" 件</td></tr>\n";
    body += L"<tr><th>文字コード</th><td>" +
            std::wstring(EncodingDetector::EncodingName(ctx.encoding)) + L"</td></tr>\n";
    body += L"<tr><th>改行コード</th><td>" +
            std::wstring(ctx.newline == NewlineKind::CRLF ? L"CRLF" :
                         (ctx.newline == NewlineKind::CR ? L"CR" : L"LF")) + L"</td></tr>\n";
    if (!result.backup_file_path.empty())
        body += L"<tr><th>バックアップ</th><td>" +
                util::EscapeHtml(result.backup_file_path) + L"</td></tr>\n";
    if (result.switch_header_updated && !result.switch_header_path.empty()) {
        body += L"<tr><th>スイッチ定義ヘッダ</th><td>" +
                util::EscapeHtml(result.switch_header_path) +
                (result.switch_header_added ? L"（スイッチを新規登録）"
                                            : L"（既存スイッチを更新）") + L"</td></tr>\n";
    }
    if (result.include_added)
        body += L"<tr><th>#include 追加</th><td>" +
                std::to_wstring(result.include_line) + L" 行目に追加</td></tr>\n";
    body += L"<tr><th>結果</th><td>" + StatusBadge(result.status) + L"</td></tr>\n";
    body += L"</table>\n";

    body += L"<h2>対象箇所</h2>\n";
    body += MatchTable(matches, false);
    body += FileDiffBody(matches, before_text, after_text);
    if (result.warning_count > 0) body += WarningList(matches);

    body += L"<p class=\"foot\">Generated by CopyMigration Switcher (Migration Switch Tool) - "
            L"Migration Switch Report</p>\n";
    return WrapHtml(L"Migration Switch Report", body);
}

// -----------------------------------------------------------------------------
// 17.2.2 手順9 統合レポート
// -----------------------------------------------------------------------------
bool DiffReportGenerator::GenerateIntegrated(const std::wstring& report_path,
                                             const OperationContext& ctx,
                                             const std::vector<ReportFileSection>& sections,
                                             const BatchExecuteResult& batch,
                                             std::wstring& out_error)
{
    out_error.clear();

    int total_matches = 0;
    int total_replaced = 0;
    for (size_t i = 0; i < sections.size(); ++i) {
        total_matches  += (int)sections[i].matches.size();
        total_replaced += sections[i].result.replaced_count;
    }

    std::wstring body;
    body += L"<h1>Migration Switch Report (一括処理)</h1>\n";

    body += L"<table class=\"meta\">\n";
    body += L"<tr><th>実行日時</th><td>" + util::EscapeHtml(ctx.started_at) + L"</td></tr>\n";
    body += L"<tr><th>対象フォルダ</th><td>" + util::EscapeHtml(batch.folder) + L"</td></tr>\n";
    body += L"<tr><th>拡張子フィルタ</th><td>" +
            util::EscapeHtml(FolderExtText(ctx.extension_filter)) + L"</td></tr>\n";
    body += L"<tr><th>サブフォルダ</th><td>" +
            std::wstring(ctx.include_subdirs ? L"含める" : L"含めない") + L"</td></tr>\n";
    body += L"<tr><th>スイッチ名</th><td><code>" + util::EscapeHtml(ctx.switch_name) + L"</code></td></tr>\n";
    body += L"<tr><th>実行モード</th><td>" +
            std::wstring(ctx.execution_mode == ExecutionMode::DryRun
                         ? L"ドライラン（ファイル未変更）" : L"本実行") + L"</td></tr>\n";
    body += L"<tr><th>対象ファイル数</th><td>" +
            std::to_wstring((int)ctx.target_files.size()) + L" ファイル</td></tr>\n";
    body += L"<tr><th>ヒットファイル数</th><td>" +
            std::to_wstring((int)sections.size()) + L" ファイル</td></tr>\n";
    body += L"<tr><th>ヒット件数</th><td>" + std::to_wstring(total_matches) + L" 件</td></tr>\n";
    body += L"<tr><th>置換件数</th><td>" + std::to_wstring(total_replaced) + L" 件</td></tr>\n";
    if (batch.switch_header_updated && !batch.switch_header_path.empty()) {
        body += L"<tr><th>スイッチ定義ヘッダ</th><td>" +
                util::EscapeHtml(batch.switch_header_path) +
                (batch.switch_header_added ? L"（スイッチを新規登録）"
                                           : L"（既存スイッチを更新）") + L"</td></tr>\n";
    }
    body += L"<tr><th>成功 / 失敗 / スキップ</th><td>" +
            std::to_wstring(batch.replaced_count > 0 ? batch.replaced_count : 0) +
            L" 件 / " + std::to_wstring(batch.error_count) + L" 件 / " +
            std::to_wstring(batch.skipped_count) + L" 件</td></tr>\n";
    body += L"<tr><th>結果</th><td>" + StatusBadge(batch.status) + L"</td></tr>\n";
    body += L"</table>\n";

    // ---- ファイル別サマリ ----
    body += L"<h2>ファイル別サマリ</h2>\n";
    body += L"<table class=\"meta\">\n";
    body += L"<tr><th>No</th><th>ファイル</th><th>ヒット</th><th>置換</th>"
            L"<th>#include</th><th>文字コード</th><th>改行</th>"
            L"<th>状態</th><th>バックアップ</th></tr>\n";
    for (size_t i = 0; i < sections.size(); ++i) {
        const ReportFileSection& s = sections[i];
        body += L"<tr><td>" + std::to_wstring((int)i + 1) + L"</td>";
        body += L"<td>" + util::EscapeHtml(util::GetFileNameOf(s.file_path)) + L"</td>";
        body += L"<td>" + std::to_wstring((int)s.matches.size()) + L"</td>";
        body += L"<td>" + std::to_wstring(s.result.replaced_count) + L"</td>";
        body += L"<td>" + (s.include_added
                           ? (L"追加 (" + std::to_wstring(s.include_line) + L" 行目)")
                           : std::wstring(L"既存")) + L"</td>";
        body += L"<td>" + util::EscapeHtml(s.encoding_name) + L"</td>";
        body += L"<td>" + util::EscapeHtml(s.newline_name) + L"</td>";
        body += L"<td>" + StatusBadge(s.result.status);
        if (!s.result.error.empty())
            body += L"<br><span style=\"color:#cf222e;font-size:11.5px;\">" +
                    util::EscapeHtml(s.result.error) + L"</span>";
        body += L"</td>";
        body += L"<td>" + util::EscapeHtml(util::GetFileNameOf(s.result.backup_file_path)) + L"</td>";
        body += L"</tr>\n";
    }
    body += L"</table>\n";

    // ---- ファイル別詳細 ----
    for (size_t i = 0; i < sections.size(); ++i) {
        const ReportFileSection& s = sections[i];
        body += L"<h3>ファイル " + std::to_wstring((int)i + 1) + L": " +
                util::EscapeHtml(s.file_path) + L"</h3>\n";

        body += L"<table class=\"meta\">\n";
        body += L"<tr><th>ヒット件数</th><td>" + std::to_wstring((int)s.matches.size()) + L" 件</td></tr>\n";
        body += L"<tr><th>置換件数</th><td>" + std::to_wstring(s.result.replaced_count) + L" 件</td></tr>\n";
        body += L"<tr><th>文字コード</th><td>" + util::EscapeHtml(s.encoding_name) + L"</td></tr>\n";
        body += L"<tr><th>改行コード</th><td>" + util::EscapeHtml(s.newline_name) + L"</td></tr>\n";
        if (s.include_added)
            body += L"<tr><th>#include 追加</th><td>" +
                    std::to_wstring(s.include_line) + L" 行目に追加</td></tr>\n";
        if (!s.result.backup_file_path.empty())
            body += L"<tr><th>バックアップ</th><td>" +
                    util::EscapeHtml(s.result.backup_file_path) + L"</td></tr>\n";
        body += L"<tr><th>状態</th><td>" + StatusBadge(s.result.status) + L"</td></tr>\n";
        if (!s.result.error.empty())
            body += L"<tr><th>エラー</th><td>" + util::EscapeHtml(s.result.error) + L"</td></tr>\n";
        body += L"</table>\n";

        body += L"<h2>対象箇所</h2>\n";
        body += MatchTable(s.matches, false);
        body += FileDiffBody(s.matches, s.before_text, s.after_text);
        if (s.result.warning_count > 0) body += WarningList(s.matches);
    }

    body += L"<p class=\"foot\">Generated by CopyMigration Switcher (Migration Switch Tool) - "
            L"Integrated Migration Switch Report</p>\n";

    std::wstring html = WrapHtml(L"Migration Switch Report (一括処理)", body);

    std::wstring dir = util::GetDirectoryOf(report_path);
    if (!dir.empty() && !util::EnsureDirectory(dir)) {
        out_error = L"レポート出力フォルダを作成できません: " + dir;
        return false;
    }

    std::string bytes = util::WideToUtf8(html);
    std::string with_bom;
    with_bom.push_back((char)0xEF);
    with_bom.push_back((char)0xBB);
    with_bom.push_back((char)0xBF);
    with_bom += bytes;

    HANDLE h = ::CreateFileW(report_path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        out_error = L"レポートファイルを作成できません: " +
                    FileManager::LastErrorMessage(::GetLastError());
        return false;
    }
    DWORD written = 0;
    BOOL ok = ::WriteFile(h, with_bom.data(), (DWORD)with_bom.size(), &written, nullptr);
    ::CloseHandle(h);
    if (!ok || written != (DWORD)with_bom.size()) {
        out_error = L"レポートの書き込みに失敗しました。";
        return false;
    }
    return true;
}

bool DiffReportGenerator::Generate(const std::wstring& report_path,
                                   const OperationContext& ctx,
                                   const std::vector<MatchResult>& matches,
                                   const std::wstring& before_text,
                                   const std::wstring& after_text,
                                   const ReplaceResult& result,
                                   std::wstring& out_error)
{
    out_error.clear();

    std::wstring html = BuildHtml(ctx, matches, before_text, after_text, result);

    std::wstring dir = util::GetDirectoryOf(report_path);
    if (!dir.empty() && !util::EnsureDirectory(dir)) {
        out_error = L"レポート出力フォルダを作成できません: " + dir;
        return false;
    }

    std::string bytes = util::WideToUtf8(html);
    std::string with_bom;
    with_bom.push_back((char)0xEF);
    with_bom.push_back((char)0xBB);
    with_bom.push_back((char)0xBF);
    with_bom += bytes;

    HANDLE h = ::CreateFileW(report_path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        out_error = L"レポートファイルを作成できません: " +
                    FileManager::LastErrorMessage(::GetLastError());
        return false;
    }
    DWORD written = 0;
    BOOL ok = ::WriteFile(h, with_bom.data(), (DWORD)with_bom.size(), &written, nullptr);
    ::CloseHandle(h);
    if (!ok || written != (DWORD)with_bom.size()) {
        out_error = L"レポートの書き込みに失敗しました。";
        return false;
    }
    return true;
}

} // namespace ms

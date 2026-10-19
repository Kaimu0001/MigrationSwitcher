// =============================================================================
// ScopeReportGenerator.cpp
// =============================================================================
#include "ScopeReportGenerator.h"
#include "FileManager.h"
#include "SpecGenerator.h"
#include "Utils.h"

namespace ms {

const wchar_t* ScopeReportGenerator::ScopeLabel(ReportScope s)
{
    switch (s) {
    case ReportScope::File:    return L"ファイル単位";
    case ReportScope::Project: return L"プロジェクト単位";
    default:                   return L"変更単位";
    }
}

std::wstring ScopeReportGenerator::ScopeTag(ReportScope s)
{
    switch (s) {
    case ReportScope::File:    return L"file";
    case ReportScope::Project: return L"project";
    default:                   return L"change";
    }
}

static std::wstring Cell(const std::wstring& s)
{
    std::wstring t = s;
    t = util::ReplaceAll(t, L"|", L"\\|");
    t = util::ReplaceAll(t, L"\r", L"");
    t = util::ReplaceAll(t, L"\n", L" ");
    return t;
}

static std::wstring Code(const std::wstring& s)
{
    std::wstring t = s;
    t = util::ReplaceAll(t, L"`", L"'");
    if (t.size() > 120) t = t.substr(0, 117) + L"...";
    return L"`" + t + L"`";
}

std::wstring ScopeReportGenerator::BuildMarkdown(const std::wstring& system_name,
                                                 const std::wstring& switch_name,
                                                 const std::wstring& created_at,
                                                 ReportScope scope,
                                                 const std::vector<ScopeEntry>& entries)
{
    int total_files = (int)entries.size();
    int total_hits = 0, total_replaced = 0, total_warn = 0, total_err = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        total_hits += (int)entries[i].matches.size();
        for (size_t k = 0; k < entries[i].matches.size(); ++k) {
            if (entries[i].matches[k].status == MatchStatus::Warning) ++total_warn;
            if (entries[i].matches[k].status == MatchStatus::Error)   ++total_err;
            if (entries[i].matches[k].selected) ++total_replaced;
        }
    }

    std::wstring md;
    md += L"# " + (system_name.empty() ? std::wstring(L"マイグレーション") : system_name)
        + L" " + ScopeLabel(scope) + L"レポート\n\n";
    md += L"| 項目 | 値 |\n|---|---|\n";
    md += L"| 作成単位 | " + std::wstring(ScopeLabel(scope)) + L" |\n";
    md += L"| スイッチ名 | " + (switch_name.empty() ? std::wstring(L"(未設定)") : switch_name) + L" |\n";
    md += L"| 作成日時 | " + created_at + L" |\n";
    md += L"| 対象ファイル数 | " + std::to_wstring(total_files) + L" |\n";
    md += L"| 検出一致数 | " + std::to_wstring(total_hits) + L" |\n";
    md += L"| 置換対象数 | " + std::to_wstring(total_replaced) + L" |\n";
    md += L"| 警告 | " + std::to_wstring(total_warn) + L" |\n";
    md += L"| エラー | " + std::to_wstring(total_err) + L" |\n\n";

    md += L"## ファイル別サマリ\n\n";
    md += L"| # | ファイル | 文字コード | 改行 | 一致 | 置換 | 状態 |\n";
    md += L"|---|---|---|---|---|---|---|\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const ScopeEntry& e = entries[i];
        int sel = 0;
        for (size_t k = 0; k < e.matches.size(); ++k) if (e.matches[k].selected) ++sel;
        md += L"| " + std::to_wstring((int)i + 1) + L" | " + Code(util::GetFileNameOf(e.file_path))
            + L" | " + e.encoding_name + L" | " + e.newline_name
            + L" | " + std::to_wstring((int)e.matches.size())
            + L" | " + std::to_wstring(sel)
            + L" | " + (e.status.empty() ? std::wstring(L"-") : e.status)
            + L" |\n";
    }
    md += L"\n";

    for (size_t i = 0; i < entries.size(); ++i) {
        const ScopeEntry& e = entries[i];
        md += L"## " + std::to_wstring((int)i + 1) + L". " + util::GetFileNameOf(e.file_path) + L"\n\n";
        md += L"- パス: `" + e.file_path + L"`\n";
        md += L"- 文字コード: " + e.encoding_name + L" / 改行: " + e.newline_name + L"\n";
        if (!e.status.empty()) md += L"- 状態: " + e.error + L"\n";
        md += L"\n";

        if (e.matches.empty()) {
            md += L"（一致なし）\n\n";
            continue;
        }
        md += L"| 行 | 種別 | 旧コード | 新コード | 状態 |\n|---|---|---|---|---|\n";
        for (size_t k = 0; k < e.matches.size(); ++k) {
            const MatchResult& m = e.matches[k];
            std::wstring st = L"対象";
            if (m.status == MatchStatus::Warning)  st = L"警告";
            else if (m.status == MatchStatus::Error) st = L"エラー";
            else if (m.status == MatchStatus::Excluded) st = L"除外";
            md += L"| " + std::to_wstring(m.start_line)
                + L" | " + Cell(m.matched_text)
                + L" | " + Code(m.matched_text)
                + L" | " + Code(m.matched_text)
                + L" | " + st + L" |\n";
        }
        md += L"\n";

        if (!e.before_text.empty() && !e.after_text.empty()) {
            md += L"<details><summary>差分（変更前 / 変更後）</summary>\n\n```diff\n";
            std::vector<std::wstring> b = util::SplitLines(e.before_text);
            std::vector<std::wstring> a = util::SplitLines(e.after_text);
            size_t n = (b.size() > a.size()) ? b.size() : a.size();
            size_t shown = 0;
            for (size_t q = 0; q < n && shown < 200; ++q, ++shown) {
                std::wstring bl = (q < b.size()) ? b[q] : std::wstring();
                std::wstring al = (q < a.size()) ? a[q] : std::wstring();
                if (bl == al) { md += L"  " + bl + L"\n"; }
                else {
                    md += L"- " + bl + L"\n";
                    md += L"+ " + al + L"\n";
                }
            }
            if (n > shown) md += L"...(以下省略)\n";
            md += L"```\n\n</details>\n\n";
        }
    }
    return md;
}

std::wstring ScopeReportGenerator::BuildHtml(const std::wstring& title, const std::wstring& markdown)
{
    std::wstring body = SpecGenerator::MarkdownToHtml(markdown);
    std::wstring html;
    html += L"<!DOCTYPE html>\n<html lang=\"ja\">\n<head>\n<meta charset=\"utf-8\">\n";
    html += L"<title>" + SpecGenerator::EscapeHtml(title) + L"</title>\n";
    html += L"<style>\n"
            L"body{font-family:'Yu Gothic UI','Meiryo',sans-serif;margin:24px;line-height:1.6;color:#222;}\n"
            L"h1{border-bottom:2px solid #444;padding-bottom:6px;}\n"
            L"h2{margin-top:28px;border-left:5px solid #4a7;padding-left:8px;}\n"
            L"table{border-collapse:collapse;margin:12px 0;font-size:14px;}\n"
            L"th,td{border:1px solid #bbb;padding:4px 8px;vertical-align:top;}\n"
            L"th{background:#eef2f5;}\n"
            L"code{background:#f4f4f4;padding:1px 4px;border-radius:3px;}\n"
            L"pre{background:#f7f7f7;border:1px solid #ddd;padding:10px;overflow:auto;}\n"
            L"</style>\n</head>\n<body>\n";
    html += body;
    html += L"</body>\n</html>\n";
    return html;
}


static std::wstring H(const std::wstring& s)
{
    std::wstring o;
    o.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L'<') o += L"&lt;";
        else if (c == L'>') o += L"&gt;";
        else if (c == L'&') o += L"&amp;";
        else if (c == L'"') o += L"&quot;";
        else o += c;
    }
    return o;
}

std::wstring ScopeReportGenerator::BuildHtmlFromEntries(const std::wstring& title,
                                                        const std::wstring& switch_name,
                                                        const std::wstring& created_at,
                                                        ReportScope scope,
                                                        const std::vector<ScopeEntry>& entries)
{
    int total_hits = 0, total_replaced = 0, total_warn = 0, total_err = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        total_hits += (int)entries[i].matches.size();
        for (size_t k = 0; k < entries[i].matches.size(); ++k) {
            if (entries[i].matches[k].status == MatchStatus::Warning) ++total_warn;
            if (entries[i].matches[k].status == MatchStatus::Error)   ++total_err;
            if (entries[i].matches[k].selected) ++total_replaced;
        }
    }

    std::wstring h;
    h += L"<!DOCTYPE html>\n<html lang=\"ja\">\n<head>\n<meta charset=\"utf-8\">\n";
    h += L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    h += L"<title>" + H(title) + L" " + ScopeLabel(scope) + L"レポート</title>\n";
    h += L"<style>\n"
         L":root{--bd:#d5dbe1;--bg:#f7f9fb;--fg:#1f2328;--mut:#5b6570;--ac:#2f6f4e;}\n"
         L"*{box-sizing:border-box;}\n"
         L"body{margin:0;padding:28px 32px 56px;font-family:'Yu Gothic UI','Meiryo',"
         L"'Segoe UI',sans-serif;line-height:1.65;color:var(--fg);background:#fff;"
         L"max-width:1200px;}\n"
         L"h1{font-size:22px;margin:0 0 4px;padding-bottom:10px;border-bottom:3px solid var(--ac);}\n"
         L"h2{font-size:16px;margin:30px 0 8px;padding-left:10px;border-left:5px solid var(--ac);}\n"
         L".meta{color:var(--mut);font-size:13px;margin:0 0 18px;}\n"
         L".warn{background:#fff6e5;border:1px solid #f0c987;padding:8px 12px;"
         L"border-radius:4px;font-size:13px;margin:12px 0;}\n"
         L"table{border-collapse:collapse;width:100%;margin:10px 0 18px;font-size:13px;}\n"
         L"th,td{border:1px solid var(--bd);padding:6px 9px;text-align:left;vertical-align:top;}\n"
         L"th{background:var(--bg);font-weight:600;white-space:nowrap;}\n"
         L"td.num{text-align:right;white-space:nowrap;}\n"
         L"code{background:#eef1f4;padding:1px 5px;border-radius:3px;font-size:12px;"
         L"font-family:Consolas,'MS Gothic',monospace;}\n"
         L"pre{background:#fafbfc;border:1px solid var(--bd);border-radius:4px;padding:12px;"
         L"overflow:auto;font-size:12px;line-height:1.5;margin:8px 0 18px;"
         L"font-family:Consolas,'MS Gothic',monospace;}\n"
         L"pre .a{color:#1a7f37;}pre .d{color:#b42318;}pre .c{color:#57606a;}\n"
         L".ok{color:#1a7f37;}.ng{color:#b42318;}.sk{color:#57606a;}\n"
         L"footer{margin-top:40px;color:var(--mut);font-size:12px;"
         L"border-top:1px solid var(--bd);padding-top:10px;}\n"
         L"</style>\n</head>\n<body>\n";

    h += L"<h1>" + H(title) + L" " + ScopeLabel(scope) + L"レポート</h1>\n";
    h += L"<p class=\"meta\">作成日時: " + H(created_at) +
         L" ／ スイッチ名: " + H(switch_name.empty() ? L"(未設定)" : switch_name) +
         L" ／ 対象ファイル: " + std::to_wstring((int)entries.size()) + L" 件</p>\n";

    h += L"<table>\n<tbody>\n";
    h += L"<tr><th style=\"width:180px\">作成単位</th><td>" + std::wstring(ScopeLabel(scope)) + L"</td></tr>\n";
    h += L"<tr><th>検出一致数</th><td class=\"num\">" + std::to_wstring(total_hits) + L"</td></tr>\n";
    h += L"<tr><th>置換対象数</th><td class=\"num\">" + std::to_wstring(total_replaced) + L"</td></tr>\n";
    h += L"<tr><th>警告 / エラー</th><td class=\"num\">" + std::to_wstring(total_warn) +
         L" / " + std::to_wstring(total_err) + L"</td></tr>\n";
    h += L"</tbody>\n</table>\n";
    if (total_err > 0 || total_warn > 0)
        h += L"<div class=\"warn\">警告 " + std::to_wstring(total_warn) + L" 件、エラー " +
             std::to_wstring(total_err) + L" 件があります。下の明細を確認してください。</div>\n";

    // ---- サマリ表 ----
    h += L"<h2>ファイル別サマリ</h2>\n<table>\n<thead><tr>"
         L"<th style=\"width:44px\">#</th><th>ファイル</th><th style=\"width:96px\">文字コード</th>"
         L"<th style=\"width:70px\">改行</th><th style=\"width:64px\">一致</th>"
         L"<th style=\"width:64px\">置換</th><th style=\"width:100px\">状態</th>"
         L"</tr></thead>\n<tbody>\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const ScopeEntry& e = entries[i];
        int sel = 0;
        for (size_t k = 0; k < e.matches.size(); ++k) if (e.matches[k].selected) ++sel;
        std::wstring st = e.status.empty() ? std::wstring(L"-") : e.status;
        h += L"<tr><td class=\"num\">" + std::to_wstring((int)i + 1) + L"</td><td><code>" +
             H(util::GetFileNameOf(e.file_path)) + L"</code></td><td>" + H(e.encoding_name) +
             L"</td><td>" + H(e.newline_name) + L"</td><td class=\"num\">" +
             std::to_wstring((int)e.matches.size()) + L"</td><td class=\"num\">" +
             std::to_wstring(sel) + L"</td><td>" + H(st) + L"</td></tr>\n";
    }
    h += L"</tbody>\n</table>\n";

    // ---- 明細 ----
    for (size_t i = 0; i < entries.size(); ++i) {
        const ScopeEntry& e = entries[i];
        h += L"<h2>" + std::to_wstring((int)i + 1) + L". " + H(util::GetFileNameOf(e.file_path)) + L"</h2>\n";
        h += L"<p class=\"meta\">" + H(e.file_path) + L"<br>文字コード: " + H(e.encoding_name) +
             L" ／ 改行: " + H(e.newline_name);
        if (!e.status.empty()) h += L" ／ 状態: " + H(e.status);
        h += L"</p>\n";
        if (!e.error.empty()) h += L"<div class=\"warn\">" + H(e.error) + L"</div>\n";

        if (e.matches.empty()) { h += L"<p>（一致なし）</p>\n"; continue; }

        h += L"<table>\n<thead><tr><th style=\"width:64px\">行</th>"
             L"<th>一致したコード</th><th style=\"width:96px\">状態</th></tr></thead>\n<tbody>\n";
        for (size_t k = 0; k < e.matches.size(); ++k) {
            const MatchResult& m = e.matches[k];
            std::wstring st = L"<span class=\"ok\">対象</span>";
            if (m.status == MatchStatus::Warning)       st = L"<span class=\"ng\">警告</span>";
            else if (m.status == MatchStatus::Error)    st = L"<span class=\"ng\">エラー</span>";
            else if (m.status == MatchStatus::Excluded) st = L"<span class=\"sk\">除外</span>";
            std::wstring txt = m.matched_text;
            if (txt.size() > 300) txt = txt.substr(0, 300) + L"...";
            h += L"<tr><td class=\"num\">" + std::to_wstring(m.start_line) +
                 L"</td><td><code>" + H(txt) + L"</code></td><td>" + st + L"</td></tr>\n";
        }
        h += L"</tbody>\n</table>\n";

        if (!e.before_text.empty() || !e.after_text.empty()) {
            h += L"<details><summary>差分（変更前 / 変更後）</summary>\n<pre>";
            std::vector<std::wstring> b = util::SplitLines(e.before_text);
            std::vector<std::wstring> a = util::SplitLines(e.after_text);
            size_t n = (b.size() > a.size()) ? b.size() : a.size();
            size_t shown = 0;
            for (size_t q = 0; q < n && shown < 400; ++q, ++shown) {
                std::wstring bl = (q < b.size()) ? b[q] : std::wstring();
                std::wstring al = (q < a.size()) ? a[q] : std::wstring();
                if (bl == al) h += L"<span class=\"c\">  " + H(bl) + L"</span>\n";
                else {
                    h += L"<span class=\"d\">- " + H(bl) + L"</span>\n";
                    h += L"<span class=\"a\">+ " + H(al) + L"</span>\n";
                }
            }
            if (n > shown) h += L"...(以下省略)\n";
            h += L"</pre>\n</details>\n";
        }
    }

    h += L"<footer>CopyMigration Switcher</footer>\n</body>\n</html>\n";
    return h;
}

std::wstring ScopeReportGenerator::MakeFileName(const std::wstring& base,
                                                ReportScope scope,
                                                const std::wstring& stamp,
                                                const wchar_t* ext)
{
    std::wstring b = base.empty() ? std::wstring(L"migration") : base;
    b = SpecGenerator::SanitizeFileName(b);
    return b + L"_" + ScopeTag(scope) + L"_" + stamp + ext;
}

bool ScopeReportGenerator::Write(const std::wstring& path, const std::wstring& content,
                                 std::wstring& out_error)
{
    return FileManager::WriteTextAtomic(path, content, TextEncoding::UTF8, false,
                                        NewlineKind::CRLF, out_error);
}

} // namespace ms

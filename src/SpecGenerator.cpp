// =============================================================================
// SpecGenerator.cpp
// =============================================================================
#include "SpecGenerator.h"
#include "AiClient.h"
#include "Utils.h"

#include <windows.h>
#include <vector>

namespace ms {

std::wstring SpecGenerator::KindLabel(int kind)
{
    return (kind == SPEC_INSPECTION) ? L"検査仕様書" : L"変更仕様書";
}

std::wstring SpecGenerator::KindTag(int kind)
{
    return (kind == SPEC_INSPECTION) ? L"inspectspec" : L"changespec";
}

// -----------------------------------------------------------------------------
// プロンプト生成
// -----------------------------------------------------------------------------
static std::wstring CommonContext(const SpecInput& in)
{
    std::wstring s;
    s += L"# 対象システム情報\n";
    s += L"- システム名: " + (in.system_name.empty() ? L"(未入力)" : in.system_name) + L"\n";
    s += L"- システム概要: " + (in.overview.empty() ? L"(未入力)" : in.overview) + L"\n";
    s += L"- 目的: " + (in.purpose.empty() ? L"(未入力)" : in.purpose) + L"\n";
    s += L"- 修正箇所概要: " + (in.modification.empty() ? L"(未入力)" : in.modification) + L"\n";
    if (!in.switch_name.empty()) s += L"- コンパイルスイッチ名: " + in.switch_name + L"\n";
    if (!in.target_summary.empty()) s += L"- 対象: " + in.target_summary + L"\n";
    if (!in.old_code.empty()) s += L"\n## 旧コード\n```\n" + in.old_code + L"\n```\n";
    if (!in.new_code.empty()) s += L"\n## 新コード\n```\n" + in.new_code + L"\n```\n";
    if (!in.switch_header_text.empty()) {
        s += L"\n## スイッチ定義ヘッダ（抜粋）\n```\n" + in.switch_header_text + L"\n```\n";
    }
    if (!in.extra.empty()) s += L"\n## 追加指示\n" + in.extra + L"\n";
    return s;
}

static const wchar_t* kCommonRules =
    L"\n# 出力の厳守事項\n"
    L"- 出力は Markdown のみ。前置き・後書き・あいさつ・「以下に示します」等は書かない。\n"
    L"- 全体を ``` で囲まない（コードブロックは必要な箇所のみ）。\n"
    L"- 見出しは ## と ### を使い、表は Markdown の表で記載する。\n"
    L"- 日本語で、レビュー可能な具体的記述にする。不明な点は「要確認」と明記し、推測で断定しない。\n"
    L"- 与えられていない事実（製品名・担当者名・日付・数値など）を創作しない。\n";

std::wstring SpecGenerator::BuildChangeSpecPrompt(const SpecInput& in)
{
    std::wstring s;
    s += L"あなたは組込み/業務システムの移行（マイグレーション）に精通したシニアエンジニアです。\n";
    s += L"以下の情報を基に、レビューと承認に使える「変更仕様書」を作成してください。\n\n";
    s += CommonContext(in);
    s += L"\n# 記載する章立て（この順序・この見出しで）\n";
    s += L"## 1. 変更概要\n";
    s += L"## 2. 変更目的・背景\n";
    s += L"## 3. 変更対象\n";
    s += L"## 4. 変更内容（変更前 / 変更後）\n";
    s += L"## 5. コンパイルスイッチ仕様（スイッチ名・既定値・切替手順）\n";
    s += L"## 6. 影響範囲と非影響範囲\n";
    s += L"## 7. 適用手順（バックアップ・適用・確認）\n";
    s += L"## 8. ロールバック手順\n";
    s += L"## 9. 未確定事項・確認事項\n";
    s += kCommonRules;
    return s;
}

std::wstring SpecGenerator::BuildInspectionSpecPrompt(const SpecInput& in)
{
    std::wstring s;
    s += L"あなたは組込み/業務システムの検証に精通したテストリードです。\n";
    s += L"以下の情報を基に、そのまま試験項目書として使える「検査仕様書」を作成してください。\n\n";
    s += CommonContext(in);
    s += L"\n# 記載する章立て（この順序・この見出しで）\n";
    s += L"## 1. 検査目的\n";
    s += L"## 2. 検査範囲と対象外\n";
    s += L"## 3. 検査観点一覧（表：ID / 観点 / 確認内容 / 期待結果）\n";
    s += L"## 4. 正常系テストケース（表：No / 手順 / 入力・前提 / 期待結果）\n";
    s += L"## 5. 異常系・境界値テストケース\n";
    s += L"## 6. スイッチ切替の確認項目（スイッチ OFF=旧コード / ON=新コード）\n";
    s += L"## 7. 回帰確認項目\n";
    s += L"## 8. 実施環境・前提条件\n";
    s += L"## 9. 合否判定基準\n";
    s += L"## 10. 証跡（エビデンス）として残すもの\n";
    s += kCommonRules;
    return s;
}

std::wstring SpecGenerator::ToCrlf(const std::wstring& s)
{
    std::wstring o;
    o.reserve(s.size() + s.size() / 16);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r') continue;
        if (s[i] == L'\n') o += L"\r\n";
        else o += s[i];
    }
    return o;
}

std::wstring SpecGenerator::BuildPromptTxt(int kind, const SpecInput& in)
{
    const std::wstring nl = L"\n";
    std::wstring label = KindLabel(kind);
    std::wstring bar(64, L'=');
    std::wstring thin(64, L'-');

    std::wstring s;
    s += bar + nl;
    s += L" " + label + L" 作成依頼（AI チャットへ貼り付けてください）" + nl;
    s += bar + nl + nl;
    s += L"【このファイルの使い方】" + nl;
    s += L"1. このファイルの内容を「すべて」コピーし、AI チャット（ChatGPT / Claude / Gemini など）に" + nl;
    s += L"   そのまま貼り付けて送信してください。" + nl;
    s += L"2. AI が " + label + L" を Markdown 形式で返します。" + nl;
    s += L"3. 返ってきた回答を「すべて」コピーし、CopyMigration Switcher の" + nl;
    s += L"   「システム情報」タブの出力欄に貼り付けて、下のボタンを押してください。" + nl;
    s += L"     ・「この内容を" + label + L"にする」 … HTML レポートを作成します" + nl;
    s += L"     ・「レポートを開く」            … 作成したレポートを開きます" + nl + nl;
    s += L"※ API キーを設定していれば、この操作をせずに「" + label + L"を生成」ボタンだけで" + nl;
    s += L"   同じ内容を自動生成できます（このファイルは API を使えない環境向けです）。" + nl + nl;
    s += thin + nl;
    s += L"▼ ここから下が AI への依頼文です（このまま送信してください）" + nl;
    s += thin + nl + nl;
    s += (kind == SPEC_INSPECTION) ? BuildInspectionSpecPrompt(in)
                                   : BuildChangeSpecPrompt(in);
    s += nl + thin + nl;
    s += L"▲ 依頼文はここまでです" + nl;
    s += thin + nl + nl;
    s += L"【回答を貼り戻すときのメモ】" + nl;
    s += L"- システム名: " + (in.system_name.empty() ? L"(未入力)" : in.system_name) + nl;
    if (!in.switch_name.empty()) s += L"- スイッチ名: " + in.switch_name + nl;
    if (!in.target_summary.empty()) s += L"- 対象: " + in.target_summary + nl;
    s += L"- 作成日時: " + in.created_at + nl;
    s += L"- 種別: " + label + nl;
    return ToCrlf(s);
}

std::wstring SpecGenerator::MakePromptFileName(const std::wstring& system_name,
                                               int kind, const std::wstring& stamp)
{
    std::wstring st;
    for (size_t i = 0; i < stamp.size(); ++i) {
        wchar_t c = stamp[i];
        if (c >= L'0' && c <= L'9') st += c;
    }
    if (st.empty()) st = L"00000000000000";
    return SanitizeFileName(system_name) + L"_" + KindTag(kind) + L"_prompt_" + st + L".txt";
}

// -----------------------------------------------------------------------------
// Markdown → HTML
// -----------------------------------------------------------------------------
std::wstring SpecGenerator::EscapeHtml(const std::wstring& s)
{
    std::wstring o;
    o.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        switch (s[i]) {
        case L'&': o += L"&amp;";  break;
        case L'<': o += L"&lt;";   break;
        case L'>': o += L"&gt;";   break;
        case L'"': o += L"&quot;"; break;
        default:   o += s[i];
        }
    }
    return o;
}

static std::wstring InlineMd(const std::wstring& s)
{
    std::wstring o = SpecGenerator::EscapeHtml(s);
    // `code`
    std::wstring r;
    size_t i = 0;
    while (i < o.size()) {
        if (o[i] == L'`') {
            size_t e = o.find(L'`', i + 1);
            if (e != std::wstring::npos) {
                r += L"<code>" + o.substr(i + 1, e - i - 1) + L"</code>";
                i = e + 1;
                continue;
            }
        }
        r += o[i];
        ++i;
    }
    // **bold**
    r = util::ReplaceAll(r, L"**", L"\x01");
    std::wstring r2;
    bool open = false;
    for (size_t k = 0; k < r.size(); ++k) {
        if (r[k] == L'\x01') { r2 += open ? L"</strong>" : L"<strong>"; open = !open; }
        else r2 += r[k];
    }
    return r2;
}

static bool IsTableSep(const std::wstring& line)
{
    std::wstring t = util::Trim(line);
    if (t.size() < 3) return false;
    if (t[0] != L'|') return false;
    int dash = 0;
    for (size_t i = 0; i < t.size(); ++i) {
        wchar_t c = t[i];
        if (c == L'-') ++dash;
        else if (c != L'|' && c != L':' && c != L' ') return false;
    }
    return dash >= 3;
}

static std::vector<std::wstring> SplitTableRow(const std::wstring& line)
{
    std::wstring t = util::Trim(line);
    if (!t.empty() && t[0] == L'|') t = t.substr(1);
    if (!t.empty() && t[t.size() - 1] == L'|') t.erase(t.size() - 1);
    std::vector<std::wstring> cells;
    std::wstring cur;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == L'|') { cells.push_back(util::Trim(cur)); cur.clear(); }
        else cur += t[i];
    }
    cells.push_back(util::Trim(cur));
    return cells;
}

std::wstring SpecGenerator::MarkdownToHtml(const std::wstring& md)
{
    std::vector<std::wstring> lines = util::SplitLines(md);
    std::wstring out;
    bool in_code = false, in_ul = false, in_ol = false;

    struct Closer {
        static void Close(std::wstring& o, bool& ul, bool& ol) {
            if (ul) { o += L"</ul>\n"; ul = false; }
            if (ol) { o += L"</ol>\n"; ol = false; }
        }
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring line = lines[i];
        std::wstring t = util::Trim(line);

        if (t.compare(0, 3, L"```") == 0) {
            Closer::Close(out, in_ul, in_ol);
            if (!in_code) { out += L"<pre><code>"; in_code = true; }
            else          { out += L"</code></pre>\n"; in_code = false; }
            continue;
        }
        if (in_code) { out += EscapeHtml(line) + L"\n"; continue; }

        if (t.empty()) { Closer::Close(out, in_ul, in_ol); continue; }

        // 水平線
        if (t == L"---" || t == L"***" || t == L"___") {
            Closer::Close(out, in_ul, in_ol);
            out += L"<hr>\n";
            continue;
        }

        // 見出し
        if (t[0] == L'#') {
            size_t n = 0;
            while (n < t.size() && t[n] == L'#') ++n;
            if (n <= 6 && n < t.size() && t[n] == L' ') {
                Closer::Close(out, in_ul, in_ol);
                int lvl = (int)n;
                out += L"<h" + std::to_wstring(lvl) + L">" +
                       InlineMd(util::Trim(t.substr(n + 1))) +
                       L"</h" + std::to_wstring(lvl) + L">\n";
                continue;
            }
        }

        // 表
        if (t.find(L'|') != std::wstring::npos && i + 1 < lines.size() &&
            IsTableSep(lines[i + 1])) {
            Closer::Close(out, in_ul, in_ol);
            std::vector<std::wstring> head = SplitTableRow(line);
            out += L"<table>\n<thead><tr>";
            for (size_t c = 0; c < head.size(); ++c)
                out += L"<th>" + InlineMd(head[c]) + L"</th>";
            out += L"</tr></thead>\n<tbody>\n";
            size_t r = i + 2;
            for (; r < lines.size(); ++r) {
                std::wstring rt = util::Trim(lines[r]);
                if (rt.empty() || rt.find(L'|') == std::wstring::npos) break;
                std::vector<std::wstring> cells = SplitTableRow(lines[r]);
                out += L"<tr>";
                for (size_t c = 0; c < cells.size(); ++c)
                    out += L"<td>" + InlineMd(cells[c]) + L"</td>";
                out += L"</tr>\n";
            }
            out += L"</tbody></table>\n";
            i = (r > 0) ? r - 1 : i;
            continue;
        }

        // 箇条書き
        bool is_ul = (t.size() > 1 && (t[0] == L'-' || t[0] == L'*' || t[0] == L'+') && t[1] == L' ');
        bool is_ol = false;
        if (!is_ul && t.size() > 2 && t[0] >= L'0' && t[0] <= L'9') {
            size_t k = 0;
            while (k < t.size() && t[k] >= L'0' && t[k] <= L'9') ++k;
            if (k < t.size() && (t[k] == L'.' || t[k] == L')') && k + 1 < t.size() && t[k + 1] == L' ')
                is_ol = true;
        }
        if (is_ul) {
            if (in_ol) { out += L"</ol>\n"; in_ol = false; }
            if (!in_ul) { out += L"<ul>\n"; in_ul = true; }
            out += L"<li>" + InlineMd(util::Trim(t.substr(2))) + L"</li>\n";
            continue;
        }
        if (is_ol) {
            if (in_ul) { out += L"</ul>\n"; in_ul = false; }
            if (!in_ol) { out += L"<ol>\n"; in_ol = true; }
            size_t k = 0;
            while (k < t.size() && t[k] != L'.' && t[k] != L')') ++k;
            out += L"<li>" + InlineMd(util::Trim(t.substr(k + 1))) + L"</li>\n";
            continue;
        }

        Closer::Close(out, in_ul, in_ol);
        out += L"<p>" + InlineMd(t) + L"</p>\n";
    }
    Closer::Close(out, in_ul, in_ol);
    if (in_code) out += L"</code></pre>\n";
    return out;
}

std::wstring SpecGenerator::WrapReportHtml(const std::wstring& title,
                                           const std::wstring& body_html,
                                           const SpecInput& in)
{
    std::wstring h;
    h += L"<!DOCTYPE html>\n<html lang=\"ja\">\n<head>\n<meta charset=\"utf-8\">\n";
    h += L"<title>" + EscapeHtml(title) + L"</title>\n";
    h += L"<style>\n"
         L"body{font-family:'Segoe UI','Meiryo',sans-serif;line-height:1.7;margin:32px auto;"
         L"max-width:960px;color:#222;padding:0 16px;}\n"
         L"h1{font-size:22px;border-bottom:2px solid #2c6cb0;padding-bottom:8px;}\n"
         L"h2{font-size:17px;margin-top:28px;border-left:4px solid #2c6cb0;padding-left:8px;}\n"
         L"h3{font-size:15px;margin-top:20px;}\n"
         L"table{border-collapse:collapse;width:100%;margin:12px 0;font-size:13px;}\n"
         L"th,td{border:1px solid #bbb;padding:6px 8px;vertical-align:top;text-align:left;}\n"
         L"th{background:#eef3f9;}\n"
         L"code{background:#f4f4f4;padding:1px 4px;border-radius:3px;font-family:Consolas,monospace;}\n"
         L"pre{background:#f7f7f7;border:1px solid #ddd;padding:10px;overflow:auto;}\n"
         L"pre code{background:none;padding:0;}\n"
         L".meta{border:1px solid #ccc;background:#fafcff;padding:12px 16px;margin:16px 0;font-size:13px;}\n"
         L".meta table{border:none;margin:0;}\n"
         L".meta td{border:none;padding:2px 10px 2px 0;}\n"
         L".note{font-size:12px;color:#666;margin-top:28px;border-top:1px dashed #bbb;padding-top:8px;}\n"
         L"</style>\n</head>\n<body>\n";
    h += L"<h1>" + EscapeHtml(title) + L"</h1>\n";
    h += L"<div class=\"meta\"><table>\n";
    h += L"<tr><td>システム名</td><td>" + EscapeHtml(in.system_name) + L"</td></tr>\n";
    if (!in.purpose.empty())
        h += L"<tr><td>目的</td><td>" + EscapeHtml(in.purpose) + L"</td></tr>\n";
    if (!in.switch_name.empty())
        h += L"<tr><td>スイッチ名</td><td>" + EscapeHtml(in.switch_name) + L"</td></tr>\n";
    if (!in.target_summary.empty())
        h += L"<tr><td>対象</td><td>" + EscapeHtml(in.target_summary) + L"</td></tr>\n";
    h += L"<tr><td>生成日時</td><td>" + EscapeHtml(in.created_at) + L"</td></tr>\n";
    h += L"<tr><td>生成方法</td><td>CopyMigration Switcher / AI 連携（Genspark API）</td></tr>\n";
    h += L"</table></div>\n";
    h += body_html;
    h += L"<div class=\"note\">本書はシステム情報と移行対象情報を基に AI が生成した草案です。"
         L"内容を確認・修正のうえ、正式な仕様書としてご利用ください。</div>\n";
    h += L"</body>\n</html>\n";
    return h;
}

// -----------------------------------------------------------------------------
// ファイル名 / 書き出し
// -----------------------------------------------------------------------------
std::wstring SpecGenerator::SanitizeFileName(const std::wstring& s)
{
    std::wstring o;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
            c == L'"' || c == L'<' || c == L'>' || c == L'|' || c < 0x20)
            continue;
        if (c == L' ') { o += L'_'; continue; }
        o += c;
    }
    if (o.empty()) o = L"system";
    if (o.size() > 60) o = o.substr(0, 60);
    return o;
}

std::wstring SpecGenerator::MakeReportFileName(const std::wstring& system_name,
                                               int kind, const std::wstring& stamp)
{
    std::wstring st;
    for (size_t i = 0; i < stamp.size(); ++i) {
        wchar_t c = stamp[i];
        if (c >= L'0' && c <= L'9') st += c;
    }
    if (st.empty()) st = L"00000000000000";
    return SanitizeFileName(system_name) + L"_" + KindTag(kind) + L"_" + st + L".html";
}

bool SpecGenerator::WriteReportFile(const std::wstring& path, const std::wstring& content,
                                    std::wstring& out_error)
{
    out_error.clear();
    std::string utf8 = AiClient::Utf8Encode(content);
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        out_error = L"レポートを書き込めません: " + path;
        return false;
    }
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    ::WriteFile(h, bom, 3, &written, nullptr);
    if (!utf8.empty()) ::WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    ::CloseHandle(h);
    return true;
}

} // namespace ms

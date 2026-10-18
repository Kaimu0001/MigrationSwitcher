// =============================================================================
// PreviewGenerator.cpp
// =============================================================================
#include "PreviewGenerator.h"
#include "SearchEngine.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

std::wstring PreviewGenerator::MakeExcerpt(const std::wstring& text,
                                           int start_line, int end_line,
                                           int context_lines,
                                           int& out_start_line)
{
    std::vector<std::wstring> lines = util::SplitLines(text);

    int from = start_line - context_lines;
    int to = end_line + context_lines;
    if (from < 1) from = 1;
    if (to > (int)lines.size()) to = (int)lines.size();
    out_start_line = from;

    if (lines.empty() || from > to) return std::wstring();

    int width = 6;
    std::wstring out;
    for (int ln = from; ln <= to; ++ln) {
        wchar_t num[16];
        swprintf(num, 16, L"%*d | ", width, ln);
        out += num;
        out += lines[(size_t)ln - 1];
        out += L"\n";
    }
    return out;
}

std::wstring PreviewGenerator::BuildSimple(const std::wstring& after_excerpt)
{
    std::wstring out;
    out += L"===== 変更後 =====\r\n\r\n";
    out += after_excerpt;
    return out;
}

std::wstring PreviewGenerator::BuildTopBottom(const std::wstring& before_excerpt,
                                              const std::wstring& after_excerpt)
{
    std::wstring out;
    out += L"===== 変更前 =====\r\n\r\n";
    out += before_excerpt;
    out += L"\r\n===== 変更後 =====\r\n\r\n";
    out += after_excerpt;
    return out;
}

std::wstring PreviewGenerator::BuildSideBySide(const std::wstring& before_excerpt,
                                               const std::wstring& after_excerpt)
{
    std::vector<std::wstring> a = util::SplitLines(before_excerpt);
    std::vector<std::wstring> b = util::SplitLines(after_excerpt);

    // 左右のカラム幅を揃える
    size_t widthA = 0;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].size() > widthA) widthA = a[i].size();
    if (widthA < 30) widthA = 30;
    if (widthA > 90) widthA = 90;

    size_t rows = a.size() > b.size() ? a.size() : b.size();

    std::wstring out;
    out += L"===== 変更前";
    for (size_t i = 8; i < widthA; ++i) out += L' ';
    out += L"| ===== 変更後\r\n\r\n";

    for (size_t i = 0; i < rows; ++i) {
        std::wstring left  = (i < a.size()) ? a[i] : std::wstring();
        std::wstring right = (i < b.size()) ? b[i] : std::wstring();

        if (left.size() > widthA) left = left.substr(0, widthA - 1) + L'~';
        out += left;
        for (size_t k = left.size(); k < widthA; ++k) out += L' ';
        out += L" | ";
        out += right;
        out += L"\r\n";
    }
    return out;
}

// -----------------------------------------------------------------------------
// 行中一致のホイスト（6.7 補足）
//   "    return old_function(x);" のように一致箇所が行の途中にある場合、
//   ブロックをそのまま差し込むと "#ifdef が行中に来て C として不正" になる。
//   そこで #ifdef/#else/#endif を行頭へ移し、行の前後のコード（"    return " と ";"）
//   を各分岐へ複製する。
// -----------------------------------------------------------------------------
static bool IsBlankRange(const std::wstring& text, size_t from, size_t to)
{
    for (size_t i = from; i < to && i < text.size(); ++i)
        if (text[i] != L' ' && text[i] != L'\t') return false;
    return true;
}

static std::wstring TrimLeftWs(const std::wstring& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    return s.substr(i);
}

static bool IsDirectiveLine(const std::wstring& line)
{
    std::wstring t = TrimLeftWs(line);
    return !t.empty() && t[0] == L'#';
}

bool PreviewGenerator::NeedsHoist(const std::wstring& text, const MatchResult& m,
                                  size_t& out_line_start, size_t& out_line_end)
{
    out_line_start = 0;
    out_line_end = 0;
    if (m.start_offset >= m.end_offset) return false;
    if (m.end_offset > text.size()) return false;

    size_t ls = m.start_offset;
    while (ls > 0 && text[ls - 1] != L'\n') --ls;
    size_t le = m.end_offset;
    while (le < text.size() && text[le] != L'\n') ++le;

    out_line_start = ls;
    out_line_end = le;
    // 行全体（前後の空白を除いて一致箇所のみ）ならホイスト不要
    return !(IsBlankRange(text, ls, m.start_offset) &&
             IsBlankRange(text, m.end_offset, le));
}

std::wstring PreviewGenerator::HoistBlock(const std::wstring& text, const MatchResult& m,
                                          const std::wstring& block,
                                          size_t line_start, size_t line_end)
{
    std::wstring prefix = text.substr(line_start, m.start_offset - line_start);
    std::wstring suffix = text.substr(m.end_offset, line_end - m.end_offset);

    std::vector<std::wstring> lines = util::SplitLines(block);
    int if_idx = -1, else_idx = -1, end_idx = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring t = TrimLeftWs(lines[i]);
        if (t.size() < 3 || t[0] != L'#') continue;
        if (if_idx < 0 && (t.compare(0, 3, L"#if") == 0)) { if_idx = (int)i; continue; }
        if (if_idx >= 0 && else_idx < 0 && t.compare(0, 5, L"#else") == 0) { else_idx = (int)i; continue; }
        if (else_idx >= 0 && end_idx < 0 && t.compare(0, 6, L"#endif") == 0) { end_idx = (int)i; break; }
    }
    if (if_idx < 0 || else_idx < 0 || end_idx < 0) return block;  // 構造不明（テンプレート等）はそのまま

    // 旧コード側の分岐を判定（一致文字列を含む方）
    std::wstring a_text, b_text;
    for (int i = if_idx + 1; i < else_idx; ++i) a_text += lines[(size_t)i] + L"\n";
    for (int i = else_idx + 1; i < end_idx; ++i) b_text += lines[(size_t)i] + L"\n";
    bool old_is_a = (a_text.find(m.matched_text) != std::wstring::npos);

    // 分岐ごとの「最初の非空行」「最後の非空行」
    int first_a = -1, last_a = -1, first_b = -1, last_b = -1;
    for (int i = if_idx + 1; i < else_idx; ++i)
        if (!util::IsBlank(lines[(size_t)i])) { if (first_a < 0) first_a = i; last_a = i; }
    for (int i = else_idx + 1; i < end_idx; ++i)
        if (!util::IsBlank(lines[(size_t)i])) { if (first_b < 0) first_b = i; last_b = i; }

    std::wstring pfx = TrimLeftWs(prefix);

    std::vector<std::wstring> out;
    for (size_t i = 0; i < lines.size(); ++i) {
        int idx = (int)i;
        std::wstring ln = lines[i];

        if (IsDirectiveLine(ln)) { out.push_back(TrimLeftWs(ln)); continue; }

        int fa = (old_is_a ? first_a : first_b);
        int fb = (old_is_a ? first_b : first_a);
        int la = (old_is_a ? last_a : last_b);
        int lb = (old_is_a ? last_b : last_a);

        if (idx == fa || idx == fb) {
            std::wstring body = TrimLeftWs(ln);
            bool already = !pfx.empty() && body.compare(0, pfx.size(), pfx) == 0;
            ln = already ? ln : (prefix + body);
        }
        if (idx == la || idx == lb) {
            bool already = !suffix.empty() && ln.size() >= suffix.size() &&
                           ln.compare(ln.size() - suffix.size(), suffix.size(), suffix) == 0;
            if (!already) ln += suffix;
        }
        out.push_back(ln);
    }
    return util::JoinLines(out, L"\n");
}

std::wstring PreviewGenerator::BuildAfterText(const std::wstring& text,
                                              std::vector<MatchResult>& matches,
                                              const std::wstring& block_for_all)
{
    std::vector<std::wstring> blk(matches.size(), block_for_all);
    return BuildAfterTextEach(text, matches, blk);
}

std::wstring PreviewGenerator::BuildAfterTextEach(const std::wstring& text,
                                                  const std::vector<MatchResult>& matches,
                                                  const std::vector<std::wstring>& blocks)
{
    std::vector<MatchResult> adj = matches;
    std::vector<std::wstring> blk = blocks;

    for (size_t i = 0; i < adj.size() && i < blk.size(); ++i) {
        size_t ls = 0, le = 0;
        if (!NeedsHoist(text, adj[i], ls, le)) continue;

        // 同一行に複数の一致がある場合は範囲が重なるためホイストしない（安全側）
        bool overlap = false;
        for (size_t j = 0; j < adj.size(); ++j) {
            if (j == i) continue;
            if (adj[j].start_offset > ls && adj[j].start_offset < le) { overlap = true; break; }
        }
        if (overlap) continue;

        std::wstring hb = HoistBlock(text, adj[i], blk[i], ls, le);
        if (hb == blk[i]) continue;
        blk[i] = hb;
        adj[i].start_offset = ls;
        adj[i].end_offset = le;
    }
    return SearchEngine::ApplyReplacementsEach(text, adj, blk);
}

bool PreviewGenerator::BuildExcerpts(const std::wstring& before_text,
                                     const std::wstring& after_text,
                                     const std::vector<MatchResult>& matches,
                                     int context_lines,
                                     std::wstring& out_before,
                                     std::wstring& out_after)
{
    out_before.clear();
    out_after.clear();
    if (matches.empty()) return false;

    int first_line = matches[0].start_line;
    int last_line = matches[0].end_line;
    for (size_t i = 1; i < matches.size(); ++i) {
        if (matches[i].start_line < first_line) first_line = matches[i].start_line;
        if (matches[i].end_line > last_line) last_line = matches[i].end_line;
    }

    int dummy = 0;
    out_before = MakeExcerpt(before_text, first_line, last_line, context_lines, dummy);

    // 変更後の行範囲: 挿入行数を加味して広めに取る
    int after_end = last_line;
    for (size_t i = 0; i < matches.size(); ++i) {
        std::wstring mt = matches[i].matched_text;
        int extra = util::CountLines(mt) + 6;   // #ifdef/#else/#endif + 余裕
        if (matches[i].end_line + extra > after_end) after_end = matches[i].end_line + extra;
    }
    int after_start = first_line - 1;
    if (after_start < 1) after_start = 1;
    out_after = MakeExcerpt(after_text, after_start, after_end, context_lines, dummy);
    return true;
}

} // namespace ms

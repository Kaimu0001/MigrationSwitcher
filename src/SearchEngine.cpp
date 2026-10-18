// =============================================================================
// SearchEngine.cpp
// =============================================================================
#include "SearchEngine.h"
#include <iterator>
#include <set>
#include <cstdio>
#include "Utils.h"
#include "CodeScope.h"

#include <algorithm>

namespace ms {

std::vector<MatchResult> SearchEngine::Search(const std::wstring& text,
                                              const std::wstring& pattern,
                                              const std::wstring& file_path)
{
    std::vector<MatchResult> results;
    if (pattern.empty() || text.empty()) return results;

    int id = 0;
    size_t pos = 0;
    while (true) {
        size_t found = text.find(pattern, pos);
        if (found == std::wstring::npos) break;

        MatchResult m;
        m.match_id = ++id;
        m.file_path = file_path;
        m.start_offset = found;
        m.end_offset = found + pattern.size();
        util::OffsetToLineCol(text, m.start_offset, m.start_line, m.start_column);
        util::OffsetToLineCol(text, m.end_offset, m.end_line, m.end_column);
        m.matched_text = pattern;
        m.match_type = MatchType::Exact;
        m.match_rate = 100;
        m.selected = false;
        m.status = MatchStatus::Target;
        results.push_back(m);

        pos = found + pattern.size();
        if (pos >= text.size()) break;
    }
    return results;
}

void SearchEngine::Renumber(std::vector<MatchResult>& matches)
{
    for (size_t i = 0; i < matches.size(); ++i)
        matches[i].match_id = (int)i + 1;
}

namespace {

struct CondDirective {
    int          line = 0;
    std::wstring kind;   // if / ifdef / ifndef / else / elif / endif
    std::wstring rest;   // 指令の後ろ（マクロ名や式）
};

bool IsIfKind(const std::wstring& k)
{
    return k == L"if" || k == L"ifdef" || k == L"ifndef";
}

bool IsIdentCh(wchar_t c)
{
    if (c >= L'0' && c <= L'9') return true;
    if (c >= L'A' && c <= L'Z') return true;
    if (c >= L'a' && c <= L'z') return true;
    if (c == L'_') return true;
    if (c >= 0x80) return true;
    return false;
}

// "#ifdef NAME" のような指令行を kind / rest に分解する
bool ParseDirective(const std::wstring& raw, std::wstring& kind, std::wstring& rest)
{
    std::wstring t = util::TrimLeft(raw);
    if (t.empty() || t[0] != L'#') return false;
    size_t i = 1;
    while (i < t.size() && (t[i] == L' ' || t[i] == L'\t')) ++i;
    size_t st = i;
    while (i < t.size() && ((t[i] >= L'a' && t[i] <= L'z') ||
                            (t[i] >= L'A' && t[i] <= L'Z'))) ++i;
    if (i == st) return false;
    kind = t.substr(st, i - st);
    for (size_t k = 0; k < kind.size(); ++k)
        if (kind[k] >= L'A' && kind[k] <= L'Z') kind[k] = (wchar_t)(kind[k] + 32);
    rest = util::Trim(t.substr(i));
    return true;
}

std::wstring LeadingIdentifier(const std::wstring& s)
{
    size_t i = 0;
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    size_t st = i;
    while (i < s.size() && IsIdentCh(s[i])) ++i;
    return s.substr(st, i - st);
}

void CollectConditionals(const std::vector<std::wstring>& lines,
                         std::vector<CondDirective>& out)
{
    out.clear();
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring kind, rest;
        if (!ParseDirective(lines[i], kind, rest)) continue;
        if (!(IsIfKind(kind) || kind == L"else" || kind == L"elif" ||
              kind == L"endif")) continue;
        CondDirective c;
        c.line = (int)i + 1;
        c.kind = kind;
        c.rest = rest;
        out.push_back(c);
    }
}

// include guard（#ifndef G / #define G … #endif）を構成する行番号を集める。
// include guard はファイル全体を包むだけで移行スイッチではないため、
// 「既存コンパイルスイッチ近傍」の警告対象から除外する。
void CollectIncludeGuardLines(const std::vector<std::wstring>& lines,
                              const std::vector<CondDirective>& conds,
                              std::set<int>& out)
{
    out.clear();
    if (conds.size() < 2) return;

    const CondDirective& first = conds[0];
    std::wstring guard;
    if (first.kind == L"ifndef") {
        guard = LeadingIdentifier(first.rest);
    } else if (first.kind == L"if") {
        size_t q = first.rest.find(L"defined");
        if (q == std::wstring::npos) return;
        if (q == 0 || first.rest[q - 1] != L'!') return;   // !defined(NAME) のみ
        std::wstring after = util::Trim(first.rest.substr(q + 7));
        if (!after.empty() && after[0] == L'(') after = after.substr(1);
        guard = LeadingIdentifier(after);
    }
    if (guard.empty()) return;

    // #ifndef の直後（次の条件指令より前）に #define GUARD があること
    int define_line = 0;
    for (int ln = first.line + 1; ln <= (int)lines.size(); ++ln) {
        std::wstring kind, rest;
        if (!ParseDirective(lines[(size_t)ln - 1], kind, rest)) continue;
        if (kind == L"define") {
            if (LeadingIdentifier(rest) == guard) define_line = ln;
            break;
        }
        if (IsIfKind(kind) || kind == L"else" || kind == L"elif" ||
            kind == L"endif") break;
    }
    if (define_line == 0) return;

    // 最初の #ifndef に対応する #endif を求め、それが最後の条件指令であること
    int depth = 0, end_line = 0;
    for (size_t i = 0; i < conds.size(); ++i) {
        if (IsIfKind(conds[i].kind)) ++depth;
        else if (conds[i].kind == L"endif") {
            --depth;
            if (depth == 0) { end_line = conds[i].line; break; }
        }
    }
    if (end_line == 0) return;
    if (conds[conds.size() - 1].line != end_line) return;

    out.insert(first.line);
    out.insert(define_line);
    out.insert(end_line);
}

std::wstring MakeDetail(int line, const std::wstring& kind, const std::wstring& rest,
                        bool inside)
{
    std::wstring d = std::to_wstring(line) + L" 行目の #" + kind;
    if (!rest.empty()) d += L" " + rest;
    if (inside) d += L" の内側です。";
    else        d += L" が対象行の近傍にあります。";
    return d;
}

} // anonymous namespace

bool SearchEngine::DetectNearbyPreprocessor(const std::wstring& text,
                                            int start_line, int end_line,
                                            int scan_lines,
                                            std::wstring& out_detail)
{
    out_detail.clear();
    if (start_line < 1) start_line = 1;
    if (end_line < start_line) end_line = start_line;

    std::vector<std::wstring> lines = util::SplitLines(text);
    if (lines.empty()) return false;

    std::vector<CondDirective> conds;
    CollectConditionals(lines, conds);
    if (conds.empty()) return false;

    std::set<int> guard_lines;
    CollectIncludeGuardLines(lines, conds, guard_lines);

    // (1) 対象行が既存の条件ブロックの内側にあるか（二重ネストの危険が最も高い）
    int depth = 0;
    int last_if_line = 0;
    std::wstring last_if_kind, last_if_rest;
    for (size_t i = 0; i < conds.size(); ++i) {
        const CondDirective& c = conds[i];
        if (c.line >= start_line) break;
        if (guard_lines.count(c.line)) continue;
        if (IsIfKind(c.kind)) {
            ++depth;
            last_if_line = c.line;
            last_if_kind = c.kind;
            last_if_rest = c.rest;
        } else if (c.kind == L"endif") {
            --depth;
            if (depth < 0) depth = 0;
        }
    }
    if (depth > 0) {
        out_detail = MakeDetail(last_if_line, last_if_kind, last_if_rest, true);
        return true;
    }

    // (2) 近傍（±scan_lines）に既存の条件指令があるか（include guard は除外）
    int from = start_line - scan_lines;
    if (from < 1) from = 1;
    int to = end_line + scan_lines;
    for (size_t i = 0; i < conds.size(); ++i) {
        const CondDirective& c = conds[i];
        if (guard_lines.count(c.line)) continue;
        if (c.line < from || c.line > to) continue;
        if (c.line >= start_line && c.line <= end_line) continue;
        out_detail = MakeDetail(c.line, c.kind, c.rest, false);
        return true;
    }
    return false;
}

bool SearchEngine::IsInsideComment(const std::wstring& text, size_t offset)
{
    if (offset > text.size()) return false;

    bool in_block = false;
    bool in_line = false;
    bool in_string = false;
    bool in_char = false;
    bool escape = false;

    for (size_t i = 0; i < offset; ++i) {
        wchar_t c = text[i];

        if (in_line) {
            if (c == L'\n') in_line = false;
            continue;
        }
        if (in_block) {
            if (c == L'*' && i + 1 < text.size() && text[i + 1] == L'/') {
                in_block = false;
                ++i;
            }
            continue;
        }
        if (in_string) {
            if (escape) { escape = false; continue; }
            if (c == L'\\') { escape = true; continue; }
            if (c == L'"') in_string = false;
            continue;
        }
        if (in_char) {
            if (escape) { escape = false; continue; }
            if (c == L'\\') { escape = true; continue; }
            if (c == L'\'') in_char = false;
            continue;
        }

        if (c == L'/' && i + 1 < text.size()) {
            if (text[i + 1] == L'/') { in_line = true; ++i; continue; }
            if (text[i + 1] == L'*') { in_block = true; ++i; continue; }
        }
        if (c == L'"') { in_string = true; continue; }
        if (c == L'\'') { in_char = true; continue; }
    }
    return in_line || in_block;
}

std::wstring SearchEngine::ApplyReplacements(const std::wstring& text,
                                             std::vector<MatchResult>& matches,
                                             const std::wstring& replacement)
{
    // 後方から置換してオフセットのずれを防ぐ
    std::vector<MatchResult*> sorted;
    sorted.reserve(matches.size());
    for (size_t i = 0; i < matches.size(); ++i)
        if (matches[i].selected) sorted.push_back(&matches[i]);

    std::sort(sorted.begin(), sorted.end(),
              [](const MatchResult* a, const MatchResult* b) {
                  return a->start_offset > b->start_offset;
              });

    std::wstring out = text;
    for (size_t i = 0; i < sorted.size(); ++i) {
        MatchResult* m = sorted[i];
        if (m->start_offset > out.size()) continue;
        if (m->end_offset > out.size()) continue;
        out.replace(m->start_offset, m->end_offset - m->start_offset, replacement);
    }
    return out;
}

std::wstring SearchEngine::ApplyReplacementsEach(const std::wstring& text,
                                                 const std::vector<MatchResult>& matches,
                                                 const std::vector<std::wstring>& blocks)
{
    // 後方から置換してオフセットのずれを防ぐ
    std::vector<size_t> order;
    order.reserve(matches.size());
    for (size_t i = 0; i < matches.size(); ++i) {
        if (!matches[i].selected) continue;
        if (i >= blocks.size()) continue;
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(),
              [&](size_t a, size_t b) {
                  return matches[a].start_offset > matches[b].start_offset;
              });

    std::wstring out = text;
    for (size_t k = 0; k < order.size(); ++k) {
        const MatchResult& m = matches[order[k]];
        if (m.start_offset > out.size()) continue;
        if (m.end_offset > out.size()) continue;
        out.replace(m.start_offset, m.end_offset - m.start_offset, blocks[order[k]]);
    }
    return out;
}

std::wstring SearchEngine::IndentHintAt(const std::wstring& text, const MatchResult& m)
{
    size_t ls = util::LineStartOffset(text, m.start_line);
    if (ls >= m.start_offset) return std::wstring();

    std::wstring indent;
    for (size_t i = ls; i < m.start_offset && i < text.size(); ++i) {
        if (text[i] == L' ' || text[i] == L'\t') indent += text[i];
        else return std::wstring();     // 行頭から一致箇所までに空白以外がある = インデントではない
    }
    return indent;
}


// -----------------------------------------------------------------------------
// 類似一致検索
// -----------------------------------------------------------------------------
std::wstring SearchEngine::NormalizeForSimilar(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;   // 先頭の空白を除去
    for (; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L' ' || c == L'\t') {
            out += L' ';
            while (i + 1 < s.size() && (s[i + 1] == L' ' || s[i + 1] == L'\t')) ++i;
        } else {
            out += c;
        }
    }
    while (!out.empty() && out[out.size() - 1] == L' ') out.erase(out.size() - 1);
    return out;
}

static void CollectBigrams(const std::wstring& s, std::vector<std::wstring>& out)
{
    out.clear();
    if (s.empty()) return;
    if (s.size() == 1) { out.push_back(s); return; }
    out.reserve(s.size() - 1);
    for (size_t i = 0; i + 1 < s.size(); ++i) out.push_back(s.substr(i, 2));
    std::sort(out.begin(), out.end());
}

int SearchEngine::SimilarityPercent(const std::wstring& a, const std::wstring& b)
{
    if (a.empty() || b.empty()) return 0;
    if (a == b) return 100;
    std::vector<std::wstring> A, B;
    CollectBigrams(a, A);
    CollectBigrams(b, B);
    if (A.empty() || B.empty()) return 0;
    std::vector<std::wstring> inter;
    inter.reserve(A.size() < B.size() ? A.size() : B.size());
    std::set_intersection(A.begin(), A.end(), B.begin(), B.end(),
                          std::back_inserter(inter));
    double dice = (2.0 * (double)inter.size()) / ((double)A.size() + (double)B.size());
    int pct = (int)(dice * 100.0 + 0.5);
    if (pct > 100) pct = 100;
    return pct;
}

// 正規化文字列と、正規化前の生オフセットとの対応表
struct SimilarNorm {
    std::wstring          norm;
    std::vector<size_t>   begin;   // norm[i] に対応する生テキストの開始位置
    std::vector<size_t>   end;     // norm[i] に対応する生テキストの終了位置（排他）
};

static void NormalizeWithMap(const std::wstring& raw, SimilarNorm& out)
{
    out.norm.clear();
    out.begin.clear();
    out.end.clear();
    size_t i = 0;
    while (i < raw.size() && (raw[i] == L' ' || raw[i] == L'\t')) ++i;
    for (; i < raw.size(); ++i) {
        wchar_t c = raw[i];
        if (c == L' ' || c == L'\t') {
            size_t st = i;
            while (i + 1 < raw.size() && (raw[i + 1] == L' ' || raw[i + 1] == L'\t')) ++i;
            out.norm += L' ';
            out.begin.push_back(st);
            out.end.push_back(i + 1);
        } else {
            out.norm += c;
            out.begin.push_back(i);
            out.end.push_back(i + 1);
        }
    }
    // 末尾の空白を除去（begin/end も揃える）
    while (!out.norm.empty() && out.norm[out.norm.size() - 1] == L' ') {
        out.norm.erase(out.norm.size() - 1);
        out.begin.pop_back();
        out.end.pop_back();
    }
}

std::vector<MatchResult> SearchEngine::SearchSimilar(const std::wstring& text,
                                                     const std::wstring& pattern,
                                                     const std::wstring& file_path,
                                                     int threshold_percent)
{
    std::vector<MatchResult> results;
    if (pattern.empty() || text.empty()) return results;
    if (pattern.find(L'\n') != std::wstring::npos) return results;   // 複数行は対象外
    if (threshold_percent < 1)   threshold_percent = 1;
    if (threshold_percent > 100) threshold_percent = 100;

    std::wstring np = NormalizeForSimilar(pattern);
    if (np.size() < 3) return results;

    std::vector<std::wstring> lines = util::SplitLines(text);
    int id = 0;
    for (size_t li = 0; li < lines.size(); ++li) {
        const std::wstring& raw = lines[li];
        if (raw.empty()) continue;
        if (raw.find(pattern) != std::wstring::npos) continue;   // 完全一致は対象外

        SimilarNorm sn;
        NormalizeWithMap(raw, sn);
        if (sn.norm.empty()) continue;
        // 安価な事前フィルタ: 長さが離れすぎている行は除外
        double ratio = (double)sn.norm.size() / (double)np.size();
        if (ratio < 0.5 || ratio > 2.5) continue;

        const size_t L = np.size();
        int    best = 0;
        size_t best_st = 0, best_len = 0;
        size_t wmin = (L > 3) ? (L - 3) : 1;
        size_t wmax = L + 3;
        for (size_t w = wmin; w <= wmax; ++w) {
            if (w == 0 || w > sn.norm.size()) continue;
            for (size_t st = 0; st + w <= sn.norm.size(); ++st) {
                int r = SimilarityPercent(np, sn.norm.substr(st, w));
                if (r > best) { best = r; best_st = st; best_len = w; }
            }
        }
        if (best < threshold_percent || best_len == 0) continue;

        size_t raw_start = sn.begin[best_st];
        size_t raw_end   = sn.end[best_st + best_len - 1];

        MatchResult m;
        m.match_id = ++id;
        m.file_path = file_path;
        m.start_offset = li == 0 ? raw_start
                                 : util::LineStartOffset(text, (int)li + 1) + raw_start;
        m.end_offset   = m.start_offset + (raw_end - raw_start);
        util::OffsetToLineCol(text, m.start_offset, m.start_line, m.start_column);
        util::OffsetToLineCol(text, m.end_offset, m.end_line, m.end_column);
        m.matched_text = raw.substr(raw_start, raw_end - raw_start);
        m.match_type = MatchType::Similar;
        m.match_rate = best;
        m.selected = false;
        m.status = MatchStatus::Warning;
        wchar_t buf[160];
        swprintf(buf, 160, L"類似一致（一致率 %d%%）。表記が異なるため内容を確認してください。", best);
        m.warning_flags.push_back(buf);
        results.push_back(m);
    }
    return results;
}

std::vector<MatchResult> SearchEngine::FilterByLineRange(const std::vector<MatchResult>& in,
                                                         int from_line, int to_line)
{
    std::vector<MatchResult> out;
    if (from_line < 1) from_line = 1;
    for (size_t i = 0; i < in.size(); ++i) {
        const MatchResult& m = in[i];
        if (m.start_line < from_line) continue;
        if (to_line > 0 && m.start_line > to_line) continue;
        out.push_back(m);
    }
    Renumber(out);
    return out;
}

std::vector<MatchResult> SearchEngine::SearchInRange(const std::wstring& text,
                                                     const std::wstring& pattern,
                                                     const std::wstring& file_path,
                                                     int from_line, int to_line)
{
    std::vector<MatchResult> all = Search(text, pattern, file_path);
    return FilterByLineRange(all, from_line, to_line);
}

std::vector<MatchResult> SearchEngine::SearchInFunction(const std::wstring& text,
                                                        const std::wstring& pattern,
                                                        const std::wstring& file_path,
                                                        const std::wstring& function_name,
                                                        std::wstring& out_error)
{
    out_error.clear();
    int s = 0, e = 0;
    if (!CodeScope::FindFunctionBody(text, function_name, s, e, out_error))
        return std::vector<MatchResult>();
    std::vector<MatchResult> r = SearchInRange(text, pattern, file_path, s, e);
    for (size_t i = 0; i < r.size(); ++i) r[i].match_type = MatchType::Exact;
    return r;
}
} // namespace ms

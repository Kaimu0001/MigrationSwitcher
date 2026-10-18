// =============================================================================
// SearchEngine.cpp
// =============================================================================
#include "SearchEngine.h"
#include "Utils.h"

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

bool SearchEngine::DetectNearbyPreprocessor(const std::wstring& text,
                                            int start_line, int end_line,
                                            int scan_lines,
                                            std::wstring& out_detail)
{
    out_detail.clear();
    std::vector<std::wstring> lines = util::SplitLines(text);

    int from = start_line - scan_lines;
    int to = end_line + scan_lines;
    if (from < 1) from = 1;
    if (to > (int)lines.size()) to = (int)lines.size();

    static const wchar_t* directives[] = {
        L"#if", L"#ifdef", L"#ifndef", L"#else", L"#elif", L"#endif"
    };

    for (int ln = from; ln <= to; ++ln) {
        if (ln >= start_line && ln <= end_line) continue;   // 対象行自体は除外
        std::wstring t = util::TrimLeft(lines[(size_t)ln - 1]);
        for (size_t d = 0; d < 6; ++d) {
            std::wstring dir = directives[d];
            if (t.size() >= dir.size() && t.compare(0, dir.size(), dir) == 0) {
                // #if は #ifdef/#ifndef も前方一致するため、語境界を確認
                if (dir == L"#if" && t.size() > 3) {
                    wchar_t c = t[3];
                    if (c == L'd' || c == L'n') continue;
                }
                wchar_t buf[128];
                swprintf(buf, 128, L"%d 行目に既存の %s を検出", ln, dir.c_str());
                out_detail = buf;
                return true;
            }
        }
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

} // namespace ms

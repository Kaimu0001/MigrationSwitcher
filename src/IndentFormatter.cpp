// =============================================================================
// IndentFormatter.cpp
// =============================================================================
#include "IndentFormatter.h"

#include <vector>

namespace ms {

std::wstring IndentFormatter::LeadingIndent(const std::wstring& line)
{
    std::wstring out;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == L' ' || line[i] == L'\t') out.push_back(line[i]);
        else break;
    }
    return out;
}

static std::vector<std::wstring> SplitLines(const std::wstring& t)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == L'\n') { out.push_back(cur); cur.clear(); }
        else cur.push_back(t[i]);
    }
    out.push_back(cur);
    return out;
}

std::wstring IndentFormatter::Dedent(const std::wstring& text)
{
    std::vector<std::wstring> lines = SplitLines(text);
    size_t common = (size_t)-1;
    for (size_t i = 0; i < lines.size(); ++i) {
        bool blank = true;
        for (size_t k = 0; k < lines[i].size(); ++k)
            if (lines[i][k] != L' ' && lines[i][k] != L'\t' && lines[i][k] != L'\r') { blank = false; break; }
        if (blank) continue;
        std::wstring ind = LeadingIndent(lines[i]);
        if (ind.size() < common) common = ind.size();
        if (common == 0) break;
    }
    if (common == (size_t)-1 || common == 0) return text;

    std::wstring out;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring ln = lines[i];
        if (!ln.empty() && ln[ln.size() - 1] == L'\r') ln.erase(ln.size() - 1);
        if (ln.size() >= common) ln = ln.substr(common);
        else ln.clear();
        out += ln;
        if (i + 1 < lines.size()) out += L"\n";
    }
    return out;
}

std::wstring IndentFormatter::FormatBlock(const std::wstring& text, const std::wstring& indent)
{
    std::wstring body = Dedent(text);
    std::vector<std::wstring> lines = SplitLines(body);
    std::wstring out;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring ln = lines[i];
        if (!ln.empty() && ln[ln.size() - 1] == L'\r') ln.erase(ln.size() - 1);
        bool blank = ln.find_first_not_of(L" \t") == std::wstring::npos;
        if (!blank) out += indent;
        out += ln;
        if (i + 1 < lines.size()) out += L"\n";
    }
    return out;
}

} // namespace ms

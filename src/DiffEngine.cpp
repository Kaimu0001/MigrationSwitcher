// =============================================================================
// DiffEngine.cpp
// =============================================================================
#include "DiffEngine.h"
#include <cstdio>
#include "Utils.h"

#include <windows.h>

namespace ms {

namespace {

void ComputeLcs(const std::vector<std::wstring>& a,
                const std::vector<std::wstring>& b,
                std::vector<std::vector<int> >& dp)
{
    size_t n = a.size(), m = b.size();
    dp.assign(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = n; i-- > 0; ) {
        for (size_t j = m; j-- > 0; ) {
            if (a[i] == b[j]) dp[i][j] = dp[i + 1][j + 1] + 1;
            else dp[i][j] = (dp[i + 1][j] >= dp[i][j + 1]) ? dp[i + 1][j] : dp[i][j + 1];
        }
    }
}

void ComputeBlockDiff(const std::vector<std::wstring>& a,
                      const std::vector<std::wstring>& b,
                      std::vector<DiffLine>& out)
{
    for (size_t i = 0; i < a.size(); ++i) {
        DiffLine d; d.type = DIFF_DELETED; d.line_no = (int)i + 1; d.text = a[i];
        out.push_back(d);
    }
    for (size_t i = 0; i < b.size(); ++i) {
        DiffLine d; d.type = DIFF_ADDED; d.line_no = (int)i + 1; d.text = b[i];
        out.push_back(d);
    }
}

} // namespace

void DiffEngine::ComputeLineDiff(const std::vector<std::wstring>& a,
                                 const std::vector<std::wstring>& b,
                                 std::vector<DiffLine>& out)
{
    out.clear();

    // 12.1: 初期版は高度な差分アルゴリズムを必須としない。規模超過時はブロック差分
    if (a.size() * b.size() > 2000000ULL) {
        ComputeBlockDiff(a, b, out);
        return;
    }

    std::vector<std::vector<int> > dp;
    ComputeLcs(a, b, dp);

    std::vector<DiffLine> raw;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            DiffLine d; d.type = DIFF_NORMAL; d.line_no = (int)j + 1; d.text = b[j];
            raw.push_back(d);
            ++i; ++j;
        } else if (dp[i + 1][j] >= dp[i][j + 1]) {
            DiffLine d; d.type = DIFF_DELETED; d.line_no = (int)i + 1; d.text = a[i];
            raw.push_back(d);
            ++i;
        } else {
            DiffLine d; d.type = DIFF_ADDED; d.line_no = (int)j + 1; d.text = b[j];
            raw.push_back(d);
            ++j;
        }
    }
    while (i < a.size()) {
        DiffLine d; d.type = DIFF_DELETED; d.line_no = (int)i + 1; d.text = a[i];
        raw.push_back(d); ++i;
    }
    while (j < b.size()) {
        DiffLine d; d.type = DIFF_ADDED; d.line_no = (int)j + 1; d.text = b[j];
        raw.push_back(d); ++j;
    }

    // 連続する削除＋追加は「変更行」としてまとめる（12.3 .changed）
    for (size_t k = 0; k < raw.size(); ) {
        if (raw[k].type == DIFF_DELETED) {
            size_t del_start = k;
            while (k < raw.size() && raw[k].type == DIFF_DELETED) ++k;
            size_t del_end = k;
            size_t add_start = k;
            while (k < raw.size() && raw[k].type == DIFF_ADDED) ++k;
            size_t add_end = k;

            size_t del_n = del_end - del_start;
            size_t add_n = add_end - add_start;
            size_t pair = (del_n < add_n) ? del_n : add_n;

            for (size_t p = 0; p < pair; ++p) {
                DiffLine d;
                d.type = DIFF_CHANGED;
                d.line_no = raw[add_start + p].line_no;
                d.text = L"- " + raw[del_start + p].text;
                out.push_back(d);
                DiffLine e;
                e.type = DIFF_CHANGED;
                e.line_no = raw[add_start + p].line_no;
                e.text = L"+ " + raw[add_start + p].text;
                out.push_back(e);
            }
            for (size_t p = pair; p < del_n; ++p) out.push_back(raw[del_start + p]);
            for (size_t p = pair; p < add_n; ++p) out.push_back(raw[add_start + p]);
        } else {
            out.push_back(raw[k]);
            ++k;
        }
    }
}

const wchar_t* DiffEngine::ClassName(DiffLineType t)
{
    switch (t) {
    case DIFF_ADDED:   return L"added";
    case DIFF_DELETED: return L"deleted";
    case DIFF_CHANGED: return L"changed";
    default:           return L"normal";
    }
}

std::wstring DiffEngine::RenderRows(const std::vector<DiffLine>& diff)
{
    std::wstring h;
    for (size_t i = 0; i < diff.size(); ++i) {
        const DiffLine& d = diff[i];
        std::wstring cls = ClassName(d.type);
        h += L"<tr class=\"" + cls + L"\"><td class=\"ln\">";
        h += (d.type == DIFF_ADDED ? std::wstring(L"+") : std::to_wstring(d.line_no));
        h += L"</td><td class=\"" + cls + L"\">";
        h += util::EscapeHtml(d.text);
        h += L"</td></tr>\n";
    }
    return h;
}

void DiffEngine::MakeExcerpt(const std::vector<std::wstring>& before,
                             const std::vector<std::wstring>& after,
                             int first_line, int last_line, int context,
                             std::vector<std::wstring>& out_before,
                             std::vector<std::wstring>& out_after,
                             int& out_before_from, int& out_after_from)
{
    out_before.clear();
    out_after.clear();

    int b_from = first_line - context; if (b_from < 1) b_from = 1;
    int b_to   = last_line + context;
    if (b_to > (int)before.size()) b_to = (int)before.size();

    for (int ln = b_from; ln <= b_to; ++ln)
        out_before.push_back(before[(size_t)ln - 1]);
    out_before_from = b_from;

    // 変更後は挿入行数ぶん後方へずれる
    int inserted = (int)after.size() - (int)before.size();
    int a_from = b_from;
    int a_to   = b_to + (inserted > 0 ? inserted : 0);
    if (a_to > (int)after.size()) a_to = (int)after.size();
    if (a_from > (int)after.size()) a_from = (int)after.size();
    if (a_from < 1) a_from = 1;

    for (int ln = a_from; ln <= a_to; ++ln)
        out_after.push_back(after[(size_t)ln - 1]);
    out_after_from = a_from;
}

std::wstring DiffEngine::PreWithLineNumbers(const std::vector<std::wstring>& lines,
                                            int first_line)
{
    std::wstring out;
    for (size_t i = 0; i < lines.size(); ++i) {
        wchar_t num[16];
        swprintf(num, 16, L"%6d | ", first_line + (int)i);
        out += util::EscapeHtml(num);
        out += util::EscapeHtml(lines[i]);
        out += L"\n";
    }
    return out;
}

} // namespace ms

// =============================================================================
// CodeScope.cpp
// =============================================================================
#include "CodeScope.h"
#include "Utils.h"

#include <vector>

namespace ms {

bool CodeScope::IsIdentifierChar(wchar_t c)
{
    if (c >= L'0' && c <= L'9') return true;
    if (c >= L'A' && c <= L'Z') return true;
    if (c >= L'a' && c <= L'z') return true;
    if (c == L'_') return true;
    if (c >= 0x80) return true;      // 日本語識別子（一部コンパイラ）も許容
    return false;
}

// 文字列・文字リテラル・コメントを読み飛ばしながら走査するための状態
namespace {
struct ScanState {
    bool in_line = false;
    bool in_block = false;
    bool in_str = false;
    bool in_chr = false;
    bool escape = false;
    void Reset() { in_line = in_block = in_str = in_chr = escape = false; }
};

// text[i] を処理する。読み飛ばした場合は true を返す（i を進める）
inline bool SkipOne(const std::wstring& t, size_t& i, ScanState& s)
{
    wchar_t c = t[i];
    if (s.in_line) { if (c == L'\n') s.in_line = false; return true; }
    if (s.in_block) {
        if (c == L'*' && i + 1 < t.size() && t[i + 1] == L'/') { s.in_block = false; ++i; }
        return true;
    }
    if (s.in_str) {
        if (s.escape) { s.escape = false; return true; }
        if (c == L'\\') { s.escape = true; return true; }
        if (c == L'"') s.in_str = false;
        return true;
    }
    if (s.in_chr) {
        if (s.escape) { s.escape = false; return true; }
        if (c == L'\\') { s.escape = true; return true; }
        if (c == L'\'') s.in_chr = false;
        return true;
    }
    if (c == L'/' && i + 1 < t.size()) {
        if (t[i + 1] == L'/') { s.in_line = true; ++i; return true; }
        if (t[i + 1] == L'*') { s.in_block = true; ++i; return true; }
    }
    if (c == L'"') { s.in_str = true; return true; }
    if (c == L'\'') { s.in_chr = true; return true; }
    return false;
}
} // anonymous namespace

bool CodeScope::MatchParen(const std::wstring& text, size_t open, size_t& close)
{
    close = 0;
    if (open >= text.size() || text[open] != L'(') return false;
    int depth = 0;
    ScanState s;
    for (size_t i = open; i < text.size(); ++i) {
        if (SkipOne(text, i, s)) continue;
        wchar_t c = text[i];
        if (c == L'(') ++depth;
        else if (c == L')') {
            --depth;
            if (depth == 0) { close = i; return true; }
        }
    }
    return false;
}

bool CodeScope::MatchBrace(const std::wstring& text, size_t open, size_t& close)
{
    close = 0;
    if (open >= text.size() || text[open] != L'{') return false;
    int depth = 0;
    ScanState s;
    for (size_t i = open; i < text.size(); ++i) {
        if (SkipOne(text, i, s)) continue;
        wchar_t c = text[i];
        if (c == L'{') ++depth;
        else if (c == L'}') {
            --depth;
            if (depth == 0) { close = i; return true; }
        }
    }
    return false;
}

bool CodeScope::FindFunctionBody(const std::wstring& text,
                                 const std::wstring& function_name,
                                 int& out_start_line, int& out_end_line,
                                 std::wstring& out_error)
{
    out_error.clear();
    out_start_line = 0;
    out_end_line = 0;

    std::wstring name = util::Trim(function_name);
    if (name.empty()) {
        out_error = L"関数名を入力してください。";
        return false;
    }
    if (text.empty()) {
        out_error = L"検索対象のテキストが空です。";
        return false;
    }

    size_t pos = 0;
    while (true) {
        size_t f = text.find(name, pos);
        if (f == std::wstring::npos) break;
        pos = f + 1;

        // 識別子の一部（my_stock_alloc など）への誤一致を除外する
        if (f > 0 && IsIdentifierChar(text[f - 1])) continue;
        size_t after = f + name.size();
        if (after < text.size() && IsIdentifierChar(text[after])) continue;

        // 直後の非空白が '(' であること（宣言・定義の引数リスト）
        size_t p = after;
        while (p < text.size() && (text[p] == L' ' || text[p] == L'\t' ||
                                   text[p] == L'\r' || text[p] == L'\n')) ++p;
        if (p >= text.size() || text[p] != L'(') continue;

        size_t pclose = 0;
        if (!MatchParen(text, p, pclose)) continue;

        // 引数リストの後、最初の '{' を探す（const / noexcept / 属性などを読み飛ばす）
        size_t b = pclose + 1;
        while (b < text.size()) {
            wchar_t c = text[b];
            if (c == L'{') break;
            if (c == L';') { b = std::wstring::npos; break; }   // 宣言のみ（本体なし）
            ++b;
        }
        if (b == std::wstring::npos || b >= text.size()) continue;

        size_t bclose = 0;
        if (!MatchBrace(text, b, bclose)) continue;

        int sl = 0, sc = 0, el = 0, ec = 0;
        util::OffsetToLineCol(text, b, sl, sc);
        util::OffsetToLineCol(text, bclose, el, ec);
        out_start_line = sl;
        out_end_line = el;
        return true;
    }

    out_error = L"関数「" + name + L"」の本体が見つかりませんでした。";
    return false;
}

std::wstring CodeScope::EnclosingFunctionName(const std::wstring& text, size_t offset)
{
    // offset より前で最も近い '{' に対応する関数名を推定する（簡易実装）
    size_t best = std::wstring::npos;
    for (size_t i = 0; i < offset && i < text.size(); ++i) {
        if (text[i] != L'{') continue;
        size_t close = 0;
        if (!MatchBrace(text, i, close)) continue;
        if (i <= offset && offset <= close) { best = i; break; }
    }
    if (best == std::wstring::npos) return std::wstring();

    // '{' の直前の識別子（引数リストの直前）を関数名とみなす
    size_t p = best;
    while (p > 0 && (text[p - 1] == L' ' || text[p - 1] == L'\t' ||
                     text[p - 1] == L'\r' || text[p - 1] == L'\n')) --p;
    if (p == 0 || text[p - 1] != L')') return std::wstring();
    size_t q = p - 1;
    int depth = 0;
    while (q > 0) {
        --q;
        if (text[q] == L')') ++depth;
        else if (text[q] == L'(') {
            if (depth == 0) break;
            --depth;
        }
    }
    size_t e = q;
    while (e > 0 && (text[e - 1] == L' ' || text[e - 1] == L'\t')) --e;
    size_t s2 = e;
    while (s2 > 0 && IsIdentifierChar(text[s2 - 1])) --s2;
    return text.substr(s2, e - s2);
}

} // namespace ms

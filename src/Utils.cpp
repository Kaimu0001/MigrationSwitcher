// =============================================================================
// Utils.cpp
// =============================================================================
#include "Utils.h"

#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <cstdio>
#include <ctime>

namespace ms {
namespace util {

// -----------------------------------------------------------------------------
// 文字コード変換
// -----------------------------------------------------------------------------
std::wstring MultiByteToWide(const std::string& s, unsigned int code_page)
{
    if (s.empty()) return std::wstring();
    int need = ::MultiByteToWideChar(code_page, 0, s.data(), (int)s.size(), nullptr, 0);
    if (need <= 0) return std::wstring();
    std::wstring out((size_t)need, L'\0');
    ::MultiByteToWideChar(code_page, 0, s.data(), (int)s.size(), &out[0], need);
    return out;
}

std::wstring Utf8ToWide(const std::string& s)
{
    return MultiByteToWide(s, CP_UTF8);
}

std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty()) return std::string();
    int need = ::WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (need <= 0) return std::string();
    std::string out((size_t)need, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], need, nullptr, nullptr);
    return out;
}

// -----------------------------------------------------------------------------
// 日時
// -----------------------------------------------------------------------------
std::wstring NowDateTimeMillis()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

std::wstring NowDateTime()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring TimestampFileName()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[32];
    swprintf(buf, 32, L"%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring TimestampDate()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[16];
    swprintf(buf, 16, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring FormatDateYMD()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[16];
    swprintf(buf, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::wstring NewOperationId()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    static unsigned int counter = 0;
    ++counter;
    wchar_t buf[64];
    swprintf(buf, 64, L"OP_%04d%02d%02d_%02d%02d%02d_%03u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
             (unsigned)(counter % 1000));
    return buf;
}

// -----------------------------------------------------------------------------
// 文字列
// -----------------------------------------------------------------------------
static bool IsSpaceW(wchar_t c)
{
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
           c == L'\f' || c == L'\v' || c == 0x3000;
}

std::wstring TrimLeft(const std::wstring& s)
{
    size_t i = 0;
    while (i < s.size() && IsSpaceW(s[i])) ++i;
    return s.substr(i);
}

std::wstring TrimRight(const std::wstring& s)
{
    size_t n = s.size();
    while (n > 0 && IsSpaceW(s[n - 1])) --n;
    return s.substr(0, n);
}

std::wstring Trim(const std::wstring& s) { return TrimRight(TrimLeft(s)); }

bool IsBlank(const std::wstring& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (!IsSpaceW(s[i])) return false;
    return true;
}

std::wstring ToUpper(const std::wstring& s)
{
    std::wstring out = s;
    for (size_t i = 0; i < out.size(); ++i) {
        wchar_t c = out[i];
        if (c >= L'a' && c <= L'z') out[i] = (wchar_t)(c - 32);
    }
    return out;
}

std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to)
{
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::wstring EscapeHtml(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        switch (c) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'"': out += L"&quot;"; break;
        case L'\'': out += L"&#39;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::wstring RepeatChar(wchar_t c, int count)
{
    if (count <= 0) return std::wstring();
    return std::wstring((size_t)count, c);
}

int CountLines(const std::wstring& s)
{
    if (s.empty()) return 0;
    int n = 1;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == L'\n') ++n;
    // 末尾が改行で終わる場合は「行」として数えない（表示上の最終行）
    if (!s.empty() && s[s.size() - 1] == L'\n') --n;
    return n < 0 ? 0 : n;
}

std::vector<std::wstring> SplitLines(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n') { out.push_back(cur); cur.clear(); }
        else cur += s[i];
    }
    out.push_back(cur);
    // 末尾改行で生じた空要素は「空行」として保持する（行番号対応を崩さない）
    return out;
}

std::wstring JoinLines(const std::vector<std::wstring>& lines, const std::wstring& nl)
{
    std::wstring out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out += nl;
        out += lines[i];
    }
    return out;
}

// -----------------------------------------------------------------------------
// 改行
// -----------------------------------------------------------------------------
std::wstring NormalizeNewlines(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L'\r') {
            if (i + 1 < s.size() && s[i + 1] == L'\n') ++i;
            out += L'\n';
        } else {
            out += c;
        }
    }
    return out;
}

std::wstring NewlineString(NewlineKind kind)
{
    switch (kind) {
    case NewlineKind::CRLF: return L"\r\n";
    case NewlineKind::CR:   return L"\r";
    default:                return L"\n";
    }
}

NewlineKind DetectNewlineKind(const std::wstring& s)
{
    size_t crlf = 0, lf = 0, cr = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\r') {
            if (i + 1 < s.size() && s[i + 1] == L'\n') { ++crlf; ++i; }
            else ++cr;
        } else if (s[i] == L'\n') {
            ++lf;
        }
    }
    if (crlf >= lf && crlf >= cr && crlf > 0) return NewlineKind::CRLF;
    if (cr > lf && cr > 0) return NewlineKind::CR;
    return NewlineKind::LF;
}

std::wstring DenormalizeNewlines(const std::wstring& s, const std::wstring& nl)
{
    if (nl == L"\n") return s;
    std::wstring out;
    out.reserve(s.size() + s.size() / 8);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n') out += nl;
        else out += s[i];
    }
    return out;
}

// -----------------------------------------------------------------------------
// 位置計算（1 起点）
// -----------------------------------------------------------------------------
void OffsetToLineCol(const std::wstring& text, size_t offset, int& line, int& col)
{
    line = 1;
    col = 1;
    if (offset > text.size()) offset = text.size();
    for (size_t i = 0; i < offset; ++i) {
        if (text[i] == L'\n') { ++line; col = 1; }
        else ++col;
    }
}

size_t LineStartOffset(const std::wstring& text, int line)
{
    if (line <= 1) return 0;
    int cur = 1;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\n') {
            ++cur;
            if (cur == line) return i + 1;
        }
    }
    return text.size();
}

int LeadingIndentWidth(const std::wstring& line, bool* has_tab)
{
    int width = 0;
    bool tab = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == L' ') ++width;
        else if (line[i] == L'\t') { tab = true; width += 4; }
        else break;
    }
    if (has_tab) *has_tab = tab;
    return width;
}

std::wstring LeadingIndentText(const std::wstring& line)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == L' ' || line[i] == L'\t')) ++i;
    return line.substr(0, i);
}

// -----------------------------------------------------------------------------
// パス
// -----------------------------------------------------------------------------
std::wstring GetDirectoryOf(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    if (p == std::wstring::npos) return std::wstring();
    return path.substr(0, p);
}

std::wstring GetFileNameOf(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    if (p == std::wstring::npos) return path;
    return path.substr(p + 1);
}

std::wstring GetExtensionOf(const std::wstring& path)
{
    std::wstring name = GetFileNameOf(path);
    size_t p = name.find_last_of(L'.');
    if (p == std::wstring::npos) return std::wstring();
    return name.substr(p);
}

std::wstring GetBaseNameNoExt(const std::wstring& path)
{
    std::wstring name = GetFileNameOf(path);
    size_t p = name.find_last_of(L'.');
    if (p == std::wstring::npos) return name;
    return name.substr(0, p);
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& name)
{
    if (dir.empty()) return name;
    std::wstring d = dir;
    wchar_t last = d[d.size() - 1];
    if (last != L'\\' && last != L'/') d += L'\\';
    return d + name;
}

std::wstring ModulePath()
{
    wchar_t buf[MAX_PATH * 2];
    ::GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    return buf;
}

std::wstring ModuleDirectory()
{
    return GetDirectoryOf(ModulePath());
}

std::wstring MakeAbsolute(const std::wstring& path)
{
    if (path.empty()) return path;
    wchar_t buf[MAX_PATH * 2];
    DWORD n = ::GetFullPathNameW(path.c_str(), MAX_PATH * 2, buf, nullptr);
    if (n == 0 || n >= MAX_PATH * 2) return path;
    return buf;
}

bool PathExists(const std::wstring& path)
{
    if (path.empty()) return false;
    DWORD attr = ::GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

bool EnsureDirectory(const std::wstring& dir)
{
    if (dir.empty()) return false;
    if (PathExists(dir)) return true;
    // 親から順に作成する
    std::wstring parent = GetDirectoryOf(dir);
    if (!parent.empty() && !PathExists(parent)) EnsureDirectory(parent);
    return ::CreateDirectoryW(dir.c_str(), nullptr) != 0;
}

std::vector<std::wstring> SplitPath(const std::wstring& path)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i < path.size(); ++i) {
        wchar_t c = path[i];
        if (c == L'\\' || c == L'/') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool IsSystemDirectoryPath(const std::wstring& path)
{
    wchar_t win[MAX_PATH * 2];
    UINT n = ::GetWindowsDirectoryW(win, MAX_PATH * 2);
    if (n == 0) return false;
    std::wstring w = ToUpper(win);
    std::wstring p = ToUpper(MakeAbsolute(path));
    if (p.size() >= w.size() && p.compare(0, w.size(), w) == 0) {
        wchar_t c = (p.size() > w.size()) ? p[w.size()] : L'\\';
        if (c == L'\\' || c == L'/') return true;
    }
    return false;
}

} // namespace util

std::string WideToMultiByte(const std::wstring& s, unsigned int code_page)
{
    if (s.empty()) return std::string();
    int need = ::WideCharToMultiByte(code_page, 0, s.data(), (int)s.size(),
                                     nullptr, 0, nullptr, nullptr);
    if (need <= 0) return std::string();
    std::string out((size_t)need, '\0');
    ::WideCharToMultiByte(code_page, 0, s.data(), (int)s.size(),
                          &out[0], need, nullptr, nullptr);
    return out;
}
} // namespace ms

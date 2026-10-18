// =============================================================================
// Utils.h
// Migration Switcher - 共通ユーティリティ
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {
namespace util {

// ---- 文字コード変換 ---------------------------------------------------------
std::wstring Utf8ToWide(const std::string& s);
std::string  WideToUtf8(const std::wstring& s);
std::wstring MultiByteToWide(const std::string& s, unsigned int code_page);
std::string  WideToMultiByte(const std::wstring& s, unsigned int code_page);

// ---- 日時 -------------------------------------------------------------------
std::wstring NowDateTime();        // yyyy-mm-dd hh:mm:ss
std::wstring NowDateTimeMillis();  // yyyy-mm-dd hh:mm:ss.mmm
std::wstring TimestampFileName();  // yyyymmdd_hhmmss
std::wstring TimestampDate();      // yyyymmdd
std::wstring FormatDateYMD();      // yyyy-mm-dd
std::wstring NewOperationId();     // OP_yyyymmdd_hhmmss_xxx

// ---- 文字列 -----------------------------------------------------------------
std::wstring Trim(const std::wstring& s);
std::wstring TrimRight(const std::wstring& s);
std::wstring TrimLeft(const std::wstring& s);
bool         IsBlank(const std::wstring& s);
std::wstring ToUpper(const std::wstring& s);
std::wstring ReplaceAll(std::wstring s, const std::wstring& from, const std::wstring& to);
std::wstring EscapeHtml(const std::wstring& s);
std::wstring RepeatChar(wchar_t c, int count);
int          CountLines(const std::wstring& s);
std::vector<std::wstring> SplitLines(const std::wstring& s);
std::wstring JoinLines(const std::vector<std::wstring>& lines, const std::wstring& nl);

// ---- 改行 -------------------------------------------------------------------
std::wstring NormalizeNewlines(const std::wstring& s);   // CRLF/CR -> LF
std::wstring NewlineString(NewlineKind kind);
NewlineKind  DetectNewlineKind(const std::wstring& s);
std::wstring DenormalizeNewlines(const std::wstring& s, const std::wstring& nl);

// ---- 位置計算 ---------------------------------------------------------------
void OffsetToLineCol(const std::wstring& text, size_t offset, int& line, int& col);
size_t LineStartOffset(const std::wstring& text, int line);
int    LeadingIndentWidth(const std::wstring& line, bool* has_tab = nullptr);
std::wstring LeadingIndentText(const std::wstring& line);

// ---- パス -------------------------------------------------------------------
std::wstring GetDirectoryOf(const std::wstring& path);
std::wstring GetFileNameOf(const std::wstring& path);
std::wstring GetBaseNameNoExt(const std::wstring& path);
std::wstring GetExtensionOf(const std::wstring& path);
std::wstring JoinPath(const std::wstring& dir, const std::wstring& name);
std::wstring ModuleDirectory();
std::wstring ModulePath();
std::wstring MakeAbsolute(const std::wstring& path);
bool         EnsureDirectory(const std::wstring& dir);
bool         PathExists(const std::wstring& path);
bool         IsSystemDirectoryPath(const std::wstring& path);
std::vector<std::wstring> SplitPath(const std::wstring& path);

} // namespace util
} // namespace ms

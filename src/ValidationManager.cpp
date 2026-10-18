// =============================================================================
// ValidationManager.cpp
// =============================================================================
#include "ValidationManager.h"
#include <cstdio>
#include "Utils.h"

namespace ms {

bool ValidationManager::IsReservedWord(const std::wstring& name)
{
    static const wchar_t* reserved[] = {
        L"if", L"else", L"for", L"while", L"do", L"switch", L"case", L"default",
        L"break", L"continue", L"return", L"goto", L"sizeof", L"typedef",
        L"struct", L"union", L"enum", L"static", L"extern", L"register",
        L"const", L"volatile", L"void", L"int", L"char", L"short", L"long",
        L"float", L"double", L"signed", L"unsigned", L"class", L"namespace",
        L"template", L"public", L"private", L"protected", L"virtual",
        L"true", L"false", L"nullptr", L"defined", L"include", L"define",
        L"undef", L"pragma", L"error", L"line", L"main", L"NULL"
    };
    std::wstring lower;
    for (size_t i = 0; i < name.size(); ++i) {
        wchar_t c = name[i];
        if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
        lower += c;
    }
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); ++i)
        if (lower == reserved[i]) return true;
    return false;
}

bool ValidationManager::IsValidSymbolChars(const std::wstring& name,
                                           int& bad_index, wchar_t& bad_char)
{
    for (size_t i = 0; i < name.size(); ++i) {
        wchar_t c = name[i];
        bool ok = (c >= L'0' && c <= L'9') ||
                  (c >= L'A' && c <= L'Z') ||
                  (c >= L'a' && c <= L'z') ||
                  c == L'_';
        if (!ok) {
            bad_index = (int)i;
            bad_char = c;
            return false;
        }
    }
    bad_index = -1;
    bad_char = 0;
    return true;
}

ValidationResult ValidationManager::ValidateSwitchName(const std::wstring& name)
{
    ValidationResult r;

    if (name.empty()) {
        r.AddError(L"コンパイルスイッチ名を入力してください。");
        return r;
    }
    if (util::IsBlank(name)) {
        r.AddError(L"コンパイルスイッチ名に空白のみは使用できません。");
        return r;
    }
    if (util::Trim(name) != name) {
        r.AddWarning(L"コンパイルスイッチ名の前後に空白があります。");
    }

    wchar_t first = name[0];
    bool firstOk = (first >= L'A' && first <= L'Z') ||
                   (first >= L'a' && first <= L'z') ||
                   first == L'_';
    if (!firstOk) {
        r.AddError(L"コンパイルスイッチ名の先頭は英字またはアンダースコアで"
                   L"始めてください。（例: MIGRATION_001）");
        return r;
    }

    int bad_index = -1;
    wchar_t bad_char = 0;
    if (!IsValidSymbolChars(name, bad_index, bad_char)) {
        wchar_t buf[256];
        swprintf(buf, 256,
                 L"コンパイルスイッチ名に使用できない文字が含まれています"
                 L"（%d 文字目: '%c'）。英数字とアンダースコアのみ使用できます。",
                 bad_index + 1, bad_char);
        r.AddError(buf);
        return r;
    }

    if (IsReservedWord(name)) {
        r.AddWarning(L"'" + name + L"' は C/C++ の予約語または標準マクロ名と"
                     L"競合する可能性があります。別の名前を推奨します。");
    }
    return r;
}

ValidationResult ValidationManager::ValidateOldCode(const std::wstring& old_code)
{
    ValidationResult r;
    if (old_code.empty()) {
        r.AddError(L"差し替え対象コードを入力してください。");
        return r;
    }
    if (util::IsBlank(old_code)) {
        r.AddError(L"差し替え対象コードが空白のみです。コードを入力してください。");
        return r;
    }
    if (!old_code.empty() && old_code[old_code.size() - 1] == L'\n') {
        r.AddWarning(L"差し替え対象コードの末尾に改行が含まれています。"
                     L"検索時に一致しない可能性があります。");
    }
    return r;
}

ValidationResult ValidationManager::ValidateNewCode(const std::wstring& old_code,
                                                    const std::wstring& new_code)
{
    ValidationResult r;
    if (new_code.empty()) {
        r.AddWarning(L"新コードが未入力です。続行しますか？");
        return r;
    }
    if (util::IsBlank(new_code)) {
        r.AddWarning(L"新コードが空白のみです。");
    }
    if (util::NormalizeNewlines(util::TrimRight(old_code)) ==
        util::NormalizeNewlines(util::TrimRight(new_code))) {
        r.AddWarning(L"新コードが差し替え対象コードと同一です。"
                     L"切り替えの意味がない可能性があります。");
    }
    if (new_code.find(L"#ifdef") != std::wstring::npos ||
        new_code.find(L"#ifndef") != std::wstring::npos) {
        r.AddWarning(L"新コードに #ifdef / #ifndef が含まれています。"
                     L"コンパイルスイッチが二重になる可能性があります。");
    }
    if (new_code.find(L"#endif") != std::wstring::npos) {
        r.AddWarning(L"新コードに #endif が含まれています。"
                     L"コンパイルスイッチが二重になる可能性があります。");
    }
    return r;
}

ValidationResult ValidationManager::ValidateTargetFile(const std::wstring& path,
                                                       const FileInfo& info)
{
    ValidationResult r;
    if (path.empty()) {
        r.AddError(L"対象ファイルを選択してください。");
        return r;
    }
    if (info.is_directory) {
        r.AddError(L"ディレクトリが指定されています。ファイルを指定してください。");
        return r;
    }
    if (!info.exists) {
        r.AddError(L"指定されたファイルが存在しません。");
        return r;
    }
    if (!info.readable) {
        r.AddError(L"ファイルを読み取れません。");
        return r;
    }
    if (info.locked) {
        r.AddError(L"ファイルが他のアプリケーションで使用中です。"
                   L"閉じてから再実行してください。");
        return r;
    }
    if (!info.writable) {
        if (info.read_only_attribute) {
            r.AddWarning(L"ファイルに読み取り専用属性が設定されています。"
                         L"実行時に属性を一時解除して書き込みます。");
        } else {
            r.AddWarning(L"ファイルに書き込めない可能性があります。");
        }
    }
    if (info.size > 20ull * 1024 * 1024) {
        r.AddWarning(L"ファイルサイズが 20MB を超えています。処理に時間がかかる"
                     L"可能性があります。");
    }
    if (util::IsSystemDirectoryPath(path)) {
        r.AddWarning(L"Windows システムディレクトリ配下のファイルです。"
                     L"変更内容を十分に確認してください。");
    }
    if (info.encoding == TextEncoding::Unknown) {
        r.AddWarning(L"文字コードを判定できませんでした。"
                     L"UTF-8 として読み込みます。");
    }
    return r;
}

std::wstring ValidationManager::BuildWarningSummary(const std::vector<std::wstring>& warnings)
{
    std::wstring out;
    for (size_t i = 0; i < warnings.size(); ++i) {
        if (i) out += L"\n";
        out += L"・" + warnings[i];
    }
    return out;
}

} // namespace ms

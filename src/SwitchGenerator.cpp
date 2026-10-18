// =============================================================================
// SwitchGenerator.cpp
// =============================================================================
#include "SwitchGenerator.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

const wchar_t* SwitchGenerator::StyleName(SwitchStyle style)
{
    switch (style) {
    case SwitchStyle::IfdefNewFirst:     return L"#ifdef 新コード優先方式 (A)";
    case SwitchStyle::IfndefOldFirst:    return L"#ifndef 旧コード優先方式 (B)";
    case SwitchStyle::IfdefWithComments: return L"コメント付き方式 (C)";
    case SwitchStyle::BlockCommentStyle: return L"開始終了コメント方式 (D)";
    case SwitchStyle::CustomTemplate:    return L"テンプレート方式";
    }
    return L"不明";
}

// -----------------------------------------------------------------------------
// 6.8 インデント自動調整
// -----------------------------------------------------------------------------
std::wstring SwitchGenerator::ApplyAutoIndent(const std::wstring& old_code,
                                              const std::wstring& new_code,
                                              std::vector<std::wstring>& out_warnings,
                                              const std::wstring& indent_hint)
{
    std::vector<std::wstring> old_lines = util::SplitLines(old_code);
    if (old_lines.empty()) return new_code;

    std::wstring indent = util::LeadingIndentText(old_lines[0]);
    // 6.8: 入力された旧コードにインデントが無い場合は、対象行の実際のインデントを利用する
    if (indent.empty()) indent = indent_hint;
    if (indent.empty()) return new_code;

    // 6.8.3 タブとスペースの混在を警告
    bool has_space = false, has_tab = false;
    for (size_t i = 0; i < indent.size(); ++i) {
        if (indent[i] == L' ') has_space = true;
        if (indent[i] == L'\t') has_tab = true;
    }
    if (has_space && has_tab) {
        out_warnings.push_back(L"インデントにタブとスペースが混在しています。"
                               L"自動調整結果を確認してください。");
    }

    std::vector<std::wstring> new_lines = util::SplitLines(new_code);

    // 6.8.3 新コードに既にインデントがある場合は調整しない
    int already_indented = 0;
    for (size_t i = 0; i < new_lines.size(); ++i) {
        if (util::IsBlank(new_lines[i])) continue;
        if (util::LeadingIndentWidth(new_lines[i]) > 0) ++already_indented;
    }
    if (already_indented > 0) {
        out_warnings.push_back(L"新コードに既にインデントが含まれています。"
                               L"インデント自動調整を適用しませんでした。");
        return new_code;
    }

    // 複数行でインデント構造が異なる場合は警告
    if (old_lines.size() > 1) {
        std::wstring first = util::LeadingIndentText(old_lines[0]);
        for (size_t i = 1; i < old_lines.size(); ++i) {
            if (util::IsBlank(old_lines[i])) continue;
            std::wstring cur = util::LeadingIndentText(old_lines[i]);
            if (cur.size() < first.size()) {
                out_warnings.push_back(L"旧コードの複数行でインデント構造が異なります。"
                                       L"自動調整結果を確認してください。");
                break;
            }
        }
    }

    std::vector<std::wstring> out;
    out.reserve(new_lines.size());
    for (size_t i = 0; i < new_lines.size(); ++i) {
        if (util::IsBlank(new_lines[i])) out.push_back(new_lines[i]);
        else out.push_back(indent + new_lines[i]);
    }
    return util::JoinLines(out, L"\n");
}

// -----------------------------------------------------------------------------
// 18章 テンプレート
// -----------------------------------------------------------------------------
bool SwitchGenerator::ValidateTemplate(const std::wstring& tpl, std::wstring& out_error)
{
    out_error.clear();
    if (tpl.empty()) {
        out_error = L"テンプレートが空です。";
        return false;
    }
    if (tpl.find(L"${NEW_CODE}") == std::wstring::npos &&
        tpl.find(L"${OLD_CODE}") == std::wstring::npos) {
        out_error = L"テンプレートに ${NEW_CODE} または ${OLD_CODE} が含まれていません。";
        return false;
    }
    if (tpl.find(L"${SWITCH}") == std::wstring::npos) {
        out_error = L"テンプレートに ${SWITCH} が含まれていません。";
        return false;
    }
    return true;
}

std::wstring SwitchGenerator::ExpandTemplate(const std::wstring& tpl,
                                             const std::wstring& switch_name,
                                             const std::wstring& old_code,
                                             const std::wstring& new_code,
                                             const std::wstring& file_path)
{
    std::wstring out = tpl;
    out = util::ReplaceAll(out, L"${SWITCH}", switch_name);
    out = util::ReplaceAll(out, L"${NEW_CODE}", new_code);
    out = util::ReplaceAll(out, L"${OLD_CODE}", old_code);
    out = util::ReplaceAll(out, L"${DATE}", util::FormatDateYMD());
    out = util::ReplaceAll(out, L"${FILE}", util::GetFileNameOf(file_path));

    wchar_t user[256];
    DWORD n = 256;
    if (!::GetUserNameW(user, &n)) user[0] = 0;
    out = util::ReplaceAll(out, L"${USER}", user);
    return out;
}

// -----------------------------------------------------------------------------
// 本体生成
// -----------------------------------------------------------------------------
std::wstring SwitchGenerator::Generate(SwitchStyle style,
                                       const std::wstring& switch_name,
                                       const std::wstring& old_code,
                                       const std::wstring& new_code,
                                       bool auto_indent,
                                       const std::wstring& custom_template,
                                       std::vector<std::wstring>& out_warnings)
{
    return GenerateEx(style, switch_name, old_code, new_code, auto_indent,
                      custom_template, std::wstring(), out_warnings);
}

std::wstring SwitchGenerator::GenerateEx(SwitchStyle style,
                                         const std::wstring& switch_name,
                                         const std::wstring& old_code,
                                         const std::wstring& new_code,
                                         bool auto_indent,
                                         const std::wstring& custom_template,
                                         const std::wstring& indent_hint,
                                         std::vector<std::wstring>& out_warnings)
{
    std::wstring nc = new_code;
    if (auto_indent) nc = ApplyAutoIndent(old_code, new_code, out_warnings, indent_hint);

    switch (style) {
    case SwitchStyle::IfdefNewFirst: {
        // 6.7.2 変換方式A
        std::wstring s;
        s += L"#ifdef " + switch_name + L"\n";
        s += nc;
        if (!nc.empty() && nc[nc.size() - 1] != L'\n') s += L"\n";
        s += L"#else\n";
        s += old_code;
        if (!old_code.empty() && old_code[old_code.size() - 1] != L'\n') s += L"\n";
        s += L"#endif";
        return s;
    }
    case SwitchStyle::IfndefOldFirst: {
        // 6.7.3 変換方式B
        std::wstring s;
        s += L"#ifndef " + switch_name + L"\n";
        s += old_code;
        if (!old_code.empty() && old_code[old_code.size() - 1] != L'\n') s += L"\n";
        s += L"#else\n";
        s += nc;
        if (!nc.empty() && nc[nc.size() - 1] != L'\n') s += L"\n";
        s += L"#endif";
        return s;
    }
    case SwitchStyle::IfdefWithComments: {
        // 6.7.4 変換方式C
        std::wstring s;
        s += L"#ifdef " + switch_name + L"  /* migration start */\n";
        s += nc;
        if (!nc.empty() && nc[nc.size() - 1] != L'\n') s += L"\n";
        s += L"#else               /* original */\n";
        s += old_code;
        if (!old_code.empty() && old_code[old_code.size() - 1] != L'\n') s += L"\n";
        s += L"#endif              /* " + switch_name + L" */";
        return s;
    }
    case SwitchStyle::BlockCommentStyle: {
        // 6.7.5 変換方式D
        std::wstring s;
        s += L"/* " + switch_name + L" start */\n";
        s += L"#ifdef " + switch_name + L"\n";
        s += nc;
        if (!nc.empty() && nc[nc.size() - 1] != L'\n') s += L"\n";
        s += L"#else\n";
        s += old_code;
        if (!old_code.empty() && old_code[old_code.size() - 1] != L'\n') s += L"\n";
        s += L"#endif\n";
        s += L"/* " + switch_name + L" end */";
        return s;
    }
    case SwitchStyle::CustomTemplate:
    default: {
        // 18章 テンプレート
        std::wstring tpl = custom_template;
        std::wstring err;
        if (!ValidateTemplate(tpl, err)) {
            // テンプレート不備時は方式Aへフォールバック
            out_warnings.push_back(L"テンプレートが不正なため方式Aを使用しました: " + err);
            return GenerateEx(SwitchStyle::IfdefNewFirst, switch_name,
                              old_code, new_code, auto_indent, L"",
                              indent_hint, out_warnings);
        }
        // テンプレート本文は改行 LF 正規化してから展開する
        tpl = util::NormalizeNewlines(tpl);
        return ExpandTemplate(tpl, switch_name, old_code, nc, L"");
    }
    }
}

} // namespace ms

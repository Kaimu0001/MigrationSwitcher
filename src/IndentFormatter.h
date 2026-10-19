// =============================================================================
// IndentFormatter.h
// 既存コードのインデントに合わせて、新コード / 追加コードを整形する
// =============================================================================
#pragma once

#include <string>

namespace ms {

class IndentFormatter {
public:
    // 各行の共通インデントを取り除く（空行は空のまま）
    static std::wstring Dedent(const std::wstring& text);

    // 共通インデントを除去し、指定のインデントを付け直す
    static std::wstring FormatBlock(const std::wstring& text, const std::wstring& indent);

    // 行頭の空白（スペース / タブ）を取り出す
    static std::wstring LeadingIndent(const std::wstring& line);
};

} // namespace ms

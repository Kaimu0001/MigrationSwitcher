// =============================================================================
// SwitchGenerator.h
// 設計書 6.7「コンパイルスイッチ変換機能」/ 6.8「インデント自動調整」/ 18章 テンプレート
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

class SwitchGenerator {
public:
    // 変換方式 A～D / テンプレート方式でコンパイルスイッチブロックを生成する
    // old_code / new_code は LF 正規化済みの内部表現
    static std::wstring Generate(SwitchStyle style,
                                 const std::wstring& switch_name,
                                 const std::wstring& old_code,
                                 const std::wstring& new_code,
                                 bool auto_indent,
                                 const std::wstring& custom_template,
                                 std::vector<std::wstring>& out_warnings);

    // indent_hint 版（6.8: 対象行の実際のインデントを自動調整に利用する）
    static std::wstring GenerateEx(SwitchStyle style,
                                   const std::wstring& switch_name,
                                   const std::wstring& old_code,
                                   const std::wstring& new_code,
                                   bool auto_indent,
                                   const std::wstring& custom_template,
                                   const std::wstring& indent_hint,
                                   std::vector<std::wstring>& out_warnings);

    // 18章 テンプレートの妥当性検証
    static bool ValidateTemplate(const std::wstring& tpl, std::wstring& out_error);

    // 18.3 置換変数の展開
    static std::wstring ExpandTemplate(const std::wstring& tpl,
                                       const std::wstring& switch_name,
                                       const std::wstring& old_code,
                                       const std::wstring& new_code,
                                       const std::wstring& file_path);

    static const wchar_t* StyleName(SwitchStyle style);

    // 6.8 インデント自動調整の適用（new_code の各行へ旧コード先頭行のインデントを付与）
    // indent_hint: 旧コードに入力インデントが無い場合に使用するインデント（対象行から取得）
    static std::wstring ApplyAutoIndent(const std::wstring& old_code,
                                        const std::wstring& new_code,
                                        std::vector<std::wstring>& out_warnings,
                                        const std::wstring& indent_hint = std::wstring());
};

} // namespace ms

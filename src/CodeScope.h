// =============================================================================
// CodeScope.h
// 後続版機能: 関数単位検索のための関数本体（{ }）検出
// 設計書 6.5「検索機能」の拡張。文字列・文字リテラル・コメントを考慮して
// 括弧の対応を取る簡易パーサ（プリプロセッサは評価しない）。
// =============================================================================
#pragma once

#include <string>

namespace ms {

class CodeScope {
public:
    // function_name の本体 { } を探し、その行範囲（1 起点・両端を含む）を返す。
    // out_start_line = '{' の行、out_end_line = 対応する '}' の行。
    static bool FindFunctionBody(const std::wstring& text,
                                 const std::wstring& function_name,
                                 int& out_start_line, int& out_end_line,
                                 std::wstring& out_error);

    // offset が属する関数名を返す（判定できない場合は空文字）
    static std::wstring EnclosingFunctionName(const std::wstring& text, size_t offset);

    // 識別子の構成文字か
    static bool IsIdentifierChar(wchar_t c);

    // open 位置の '(' / '{' に対応する閉じ位置を返す（文字列・コメントを考慮）
    static bool MatchParen(const std::wstring& text, size_t open, size_t& close);
    static bool MatchBrace(const std::wstring& text, size_t open, size_t& close);
};

} // namespace ms

// =============================================================================
// SpecGenerator.h
// システム情報 + AI API から「変更仕様書」「検査仕様書」を生成する
//   - プロンプト生成 / Markdown → HTML 変換 / レポート書き出し
// =============================================================================
#pragma once

#include <string>

namespace ms {

// 仕様書生成の入力（システム情報 + 移行対象の情報）
struct SpecInput {
    std::wstring system_name;         // システム名
    std::wstring overview;            // システム概要
    std::wstring purpose;             // 目的
    std::wstring modification;        // 修正箇所概要
    std::wstring extra;               // 追加指示（任意）
    std::wstring switch_name;         // スイッチ名
    std::wstring old_code;            // 旧コード
    std::wstring new_code;            // 新コード
    std::wstring target_summary;      // 対象ファイル / 件数の要約
    std::wstring switch_header_text;  // スイッチ定義ヘッダの内容（任意）
    std::wstring created_at;          // 生成日時
};

enum SpecKind {
    SPEC_CHANGE     = 0,   // 変更仕様書
    SPEC_INSPECTION = 1    // 検査仕様書
};

class SpecGenerator {
public:
    static std::wstring KindLabel(int kind);           // 変更仕様書 / 検査仕様書
    static std::wstring KindTag(int kind);             // changespec / inspectspec

    static std::wstring BuildChangeSpecPrompt(const SpecInput& in);
    static std::wstring BuildInspectionSpecPrompt(const SpecInput& in);

    // AI チャットへそのまま貼り付けられる依頼文テキスト（API を使わない運用向け）
    //   ・冒頭に「使い方」、本文に AI への依頼文、末尾に貼り戻し先のメモを付ける
    static std::wstring BuildPromptTxt(int kind, const SpecInput& in);
    static std::wstring MakePromptFileName(const std::wstring& system_name,
                                           int kind, const std::wstring& stamp);
    // 改行を CRLF に統一する（Windows のメモ帳・チャット貼り付け向け）
    static std::wstring ToCrlf(const std::wstring& s);

    static std::wstring EscapeHtml(const std::wstring& s);
    static std::wstring MarkdownToHtml(const std::wstring& md);
    static std::wstring WrapReportHtml(const std::wstring& title,
                                       const std::wstring& body_html,
                                       const SpecInput& in);

    static std::wstring SanitizeFileName(const std::wstring& s);
    static std::wstring MakeReportFileName(const std::wstring& system_name,
                                           int kind, const std::wstring& stamp);
    static bool WriteReportFile(const std::wstring& path, const std::wstring& content,
                                std::wstring& out_error);
};

} // namespace ms

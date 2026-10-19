// =============================================================================
// ScopeReportGenerator.h
// 追加機能: レポート / 仕様書の作成単位（変更単位 / ファイル単位 / プロジェクト単位）
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

struct ScopeEntry {
    std::wstring file_path;
    std::wstring encoding_name;
    std::wstring newline_name;
    std::wstring before_text;
    std::wstring after_text;
    std::wstring status;
    std::wstring error;
    std::vector<MatchResult> matches;
};

class ScopeReportGenerator {
public:
    static const wchar_t* ScopeLabel(ReportScope s);
    static std::wstring   ScopeTag(ReportScope s);

    static std::wstring BuildMarkdown(const std::wstring& system_name,
                                      const std::wstring& switch_name,
                                      const std::wstring& created_at,
                                      ReportScope scope,
                                      const std::vector<ScopeEntry>& entries);

    static std::wstring BuildHtml(const std::wstring& title, const std::wstring& markdown);

    // ファイル単位 / プロジェクト単位レポート用の HTML を直接組み立てる
    // （Markdown 変換を経由しないので表・差分が崩れない）
    static std::wstring BuildHtmlFromEntries(const std::wstring& title,
                                             const std::wstring& switch_name,
                                             const std::wstring& created_at,
                                             ReportScope scope,
                                             const std::vector<ScopeEntry>& entries);

    static std::wstring MakeFileName(const std::wstring& base,
                                     ReportScope scope,
                                     const std::wstring& stamp,
                                     const wchar_t* ext);

    static bool Write(const std::wstring& path, const std::wstring& content,
                      std::wstring& out_error);
};

} // namespace ms

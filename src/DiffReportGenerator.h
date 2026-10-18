// =============================================================================
// DiffReportGenerator.h
// 設計書 6.12「差分レポート出力機能」/ 12章「差分生成設計」
// 単一ファイル版と、17章 一括処理用の統合レポート版を提供する
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

// 統合レポートの 1 ファイル分セクション
struct ReportFileSection {
    std::wstring file_path;
    std::vector<MatchResult> matches;
    std::wstring before_text;
    std::wstring after_text;
    FileReplaceResult result;      // 実行結果（未実行時は status が空）
    std::wstring encoding_name;
    std::wstring newline_name;
    bool         include_added = false;
    int          include_line = 0;
};

class DiffReportGenerator {
public:
    // 6.12 単一ファイル版
    static bool Generate(const std::wstring& report_path,
                         const OperationContext& ctx,
                         const std::vector<MatchResult>& matches,
                         const std::wstring& before_text,
                         const std::wstring& after_text,
                         const ReplaceResult& result,
                         std::wstring& out_error);

    // 17.2.2 手順9 統合レポート（全ファイルを 1 つの HTML にまとめる）
    static bool GenerateIntegrated(const std::wstring& report_path,
                                   const OperationContext& ctx,
                                   const std::vector<ReportFileSection>& sections,
                                   const BatchExecuteResult& batch,
                                   std::wstring& out_error);

    // reports/yyyyMMdd_hhmmss_<basename>_diff.html
    static std::wstring BuildReportPath(const std::wstring& report_dir,
                                        const std::wstring& target_file,
                                        const std::wstring& stamp);
    // reports/yyyyMMdd_hhmmss_<foldername>_batch_diff.html
    static std::wstring BuildIntegratedReportPath(const std::wstring& report_dir,
                                                  const std::wstring& folder,
                                                  const std::wstring& stamp);

    static std::wstring BuildHtml(const OperationContext& ctx,
                                  const std::vector<MatchResult>& matches,
                                  const std::wstring& before_text,
                                  const std::wstring& after_text,
                                  const ReplaceResult& result);
};

} // namespace ms

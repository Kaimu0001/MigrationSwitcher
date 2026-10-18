// =============================================================================
// ResultExporter.h
// 後続版機能: 検索結果のエクスポート（CSV / Markdown / JSON）
// 設計書 6.5.4「検索結果」の外部出力。CSV は Excel で開けるよう
// UTF-8 (BOM) / Shift_JIS / EUC-JP を選択できる。
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

enum class ExportFormat {
    Csv = 0,
    Markdown,
    Json
};

class ResultExporter {
public:
    // CSV のフィールドをエスケープする（, " 改行 を含む場合は " で囲む）
    static std::wstring CsvEscape(const std::wstring& s);
    static const wchar_t* FormatName(ExportFormat f);
    static const wchar_t* ExtensionOf(ExportFormat f);

    static std::wstring BuildCsv(const std::vector<MatchResult>& matches,
                                 const std::wstring& operation_id);
    static std::wstring BuildMarkdown(const std::vector<MatchResult>& matches,
                                      const std::wstring& operation_id);
    static std::wstring BuildJson(const std::vector<MatchResult>& matches,
                                  const std::wstring& operation_id);

    // encoding_mode: auto / utf8 / utf8bom / sjis / eucjp
    //   CSV は Excel 向けに UTF-8 BOM を既定とする。
    static bool Export(const std::wstring& path,
                       const std::vector<MatchResult>& matches,
                       const std::wstring& operation_id,
                       ExportFormat format,
                       const std::wstring& encoding_mode,
                       std::wstring& out_error);

    // reports/yyyyMMdd_hhmmss_<base>_result.<ext>
    static std::wstring BuildExportPath(const std::wstring& dir,
                                        const std::wstring& stamp,
                                        ExportFormat format,
                                        const std::wstring& base_name);
};

} // namespace ms

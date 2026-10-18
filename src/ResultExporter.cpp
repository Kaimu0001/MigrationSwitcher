// =============================================================================
// ResultExporter.cpp
// =============================================================================
#include "ResultExporter.h"
#include "EncodingDetector.h"
#include "DiffReportGenerator.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

const wchar_t* ResultExporter::FormatName(ExportFormat f)
{
    switch (f) {
    case ExportFormat::Csv:      return L"CSV";
    case ExportFormat::Markdown: return L"Markdown";
    case ExportFormat::Json:     return L"JSON";
    default:                     return L"(不明)";
    }
}

const wchar_t* ResultExporter::ExtensionOf(ExportFormat f)
{
    switch (f) {
    case ExportFormat::Csv:      return L"csv";
    case ExportFormat::Markdown: return L"md";
    case ExportFormat::Json:     return L"json";
    default:                     return L"txt";
    }
}

std::wstring ResultExporter::CsvEscape(const std::wstring& s)
{
    bool need = false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L',' || c == L'"' || c == L'\n' || c == L'\r') { need = true; break; }
    }
    if (!need) return s;

    std::wstring o;
    o += L'"';
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'"') o += L"\"\"";
        else if (s[i] == L'\r') continue;
        else o += s[i];
    }
    o += L'"';
    return o;
}

static std::wstring MatchTypeName(const MatchResult& m)
{
    switch (m.match_type) {
    case MatchType::Exact:      return L"完全一致";
    case MatchType::Normalized: return L"正規化一致";
    case MatchType::Similar:    return L"類似一致";
    default:                    return L"不明";
    }
}

static std::wstring MatchStatusName(const MatchResult& m)
{
    switch (m.status) {
    case MatchStatus::Target:   return L"対象";
    case MatchStatus::Excluded: return L"除外";
    case MatchStatus::Warning:  return L"警告";
    case MatchStatus::Error:    return L"エラー";
    default:                    return L"不明";
    }
}

std::wstring ResultExporter::BuildCsv(const std::vector<MatchResult>& matches,
                                      const std::wstring& operation_id)
{
    std::wstring out;
    out += L"No,選択,ファイル名,フルパス,行番号,列番号,一致種別,一致率(%),状態,一致文字列,警告\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        const MatchResult& m = matches[i];
        std::wstring warns;
        for (size_t k = 0; k < m.warning_flags.size(); ++k) {
            if (k) warns += L" / ";
            warns += m.warning_flags[k];
        }
        out += std::to_wstring(m.match_id) + L",";
        out += std::wstring(m.selected ? L"○" : L"") + L",";
        out += CsvEscape(util::GetFileNameOf(m.file_path)) + L",";
        out += CsvEscape(m.file_path) + L",";
        out += std::to_wstring(m.start_line) + L",";
        out += std::to_wstring(m.start_column) + L",";
        out += MatchTypeName(m) + L",";
        out += std::to_wstring(m.match_rate) + L",";
        out += MatchStatusName(m) + L",";
        out += CsvEscape(m.matched_text) + L",";
        out += CsvEscape(warns);
        if (i + 1 < matches.size()) out += L"\n";
    }
    return out;
}

std::wstring ResultExporter::BuildMarkdown(const std::vector<MatchResult>& matches,
                                           const std::wstring& operation_id)
{
    std::wstring md;
    md += L"# 検索結果\n\n";
    md += L"- 操作 ID: " + operation_id + L"\n";
    md += L"- 件数: " + std::to_wstring((int)matches.size()) + L" 件\n\n";
    md += L"| No | 選択 | ファイル名 | 行 | 列 | 一致種別 | 一致率 | 状態 | 一致文字列 |\n";
    md += L"|---:|:---:|---|---:|---:|---|---:|---|---|\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        const MatchResult& m = matches[i];
        std::wstring cell;
        for (size_t k = 0; k < m.matched_text.size(); ++k) {
            wchar_t c = m.matched_text[k];
            if (c == L'|') cell += L"\\|";
            else if (c == L'\n' || c == L'\r' || c == L'\t') cell += L' ';
            else cell += c;
        }
        md += L"| " + std::to_wstring(m.match_id) +
              L" | " + (m.selected ? L"○" : L"") +
              L" | " + util::GetFileNameOf(m.file_path) +
              L" | " + std::to_wstring(m.start_line) +
              L" | " + std::to_wstring(m.start_column) +
              L" | " + MatchTypeName(m) +
              L" | " + std::to_wstring(m.match_rate) + L"%" +
              L" | " + MatchStatusName(m) +
              L" | `" + cell + L"` |\n";
    }
    return md;
}

std::wstring ResultExporter::BuildJson(const std::vector<MatchResult>& matches,
                                       const std::wstring& operation_id)
{
    std::wstring j;
    j += L"{\n";
    j += L"  \"report_type\": \"search_result\",\n";
    j += L"  \"operation_id\": \"" + DiffReportGenerator::JsonEscape(operation_id) + L"\",\n";
    j += L"  \"total\": " + std::to_wstring((int)matches.size()) + L",\n";
    j += L"  \"matches\": [\n";
    for (size_t i = 0; i < matches.size(); ++i) {
        const MatchResult& m = matches[i];
        std::wstring type = L"exact";
        if (m.match_type == MatchType::Normalized) type = L"normalized";
        else if (m.match_type == MatchType::Similar) type = L"similar";
        std::wstring st = L"target";
        if (m.status == MatchStatus::Excluded) st = L"excluded";
        else if (m.status == MatchStatus::Warning) st = L"warning";
        else if (m.status == MatchStatus::Error) st = L"error";
        j += L"    { \"no\": " + std::to_wstring(m.match_id) +
             L", \"selected\": " + std::wstring(m.selected ? L"true" : L"false") +
             L", \"file\": \"" + DiffReportGenerator::JsonEscape(m.file_path) +
             L"\", \"line\": " + std::to_wstring(m.start_line) +
             L", \"column\": " + std::to_wstring(m.start_column) +
             L", \"match_type\": \"" + type +
             L"\", \"match_rate\": " + std::to_wstring(m.match_rate) +
             L", \"status\": \"" + st +
             L"\", \"text\": \"" + DiffReportGenerator::JsonEscape(m.matched_text) +
             L"\", \"warning_count\": " + std::to_wstring((int)m.warning_flags.size()) + L" }";
        if (i + 1 < matches.size()) j += L",";
        j += L"\n";
    }
    j += L"  ]\n}\n";
    return j;
}

std::wstring ResultExporter::BuildExportPath(const std::wstring& dir,
                                             const std::wstring& stamp,
                                             ExportFormat format,
                                             const std::wstring& base_name)
{
    std::wstring base = base_name.empty() ? std::wstring(L"result") : base_name;
    std::wstring name = stamp + L"_" + base + L"_result." + ExtensionOf(format);
    return util::JoinPath(dir, name);
}

bool ResultExporter::Export(const std::wstring& path,
                            const std::vector<MatchResult>& matches,
                            const std::wstring& operation_id,
                            ExportFormat format,
                            const std::wstring& encoding_mode,
                            std::wstring& out_error)
{
    out_error.clear();
    std::wstring dir = util::GetDirectoryOf(path);
    if (!dir.empty() && !util::EnsureDirectory(dir)) {
        out_error = L"出力先フォルダを作成できません: " + dir;
        return false;
    }

    std::wstring text;
    switch (format) {
    case ExportFormat::Csv:      text = BuildCsv(matches, operation_id); break;
    case ExportFormat::Markdown: text = BuildMarkdown(matches, operation_id); break;
    case ExportFormat::Json:     text = BuildJson(matches, operation_id); break;
    default:                     text = BuildCsv(matches, operation_id); break;
    }

    // JSON は仕様上 BOM を付けない。CSV は Excel 向けに UTF-8 BOM を既定とする。
    std::wstring mode = util::ToUpper(encoding_mode);
    TextEncoding enc = TextEncoding::UTF8BOM;
    bool with_bom = true;
    if (format == ExportFormat::Json) {
        enc = TextEncoding::UTF8;
        with_bom = false;
        if (mode == L"SJIS" || mode == L"SHIFT_JIS" || mode == L"CP932") enc = TextEncoding::ShiftJIS;
        else if (mode == L"EUCJP" || mode == L"EUC-JP")                    enc = TextEncoding::EucJp;
    } else {
        if (mode == L"SJIS" || mode == L"SHIFT_JIS" || mode == L"CP932") { enc = TextEncoding::ShiftJIS; with_bom = false; }
        else if (mode == L"EUCJP" || mode == L"EUC-JP")                  { enc = TextEncoding::EucJp;    with_bom = false; }
        else if (mode == L"UTF8" || mode == L"UTF-8")                    { enc = TextEncoding::UTF8;     with_bom = false; }
        else                                                             { enc = TextEncoding::UTF8BOM;  with_bom = true;  }
    }

    if (!FileManager::WriteTextAtomic(path, text, enc, with_bom, NewlineKind::CRLF, out_error)) {
        if (out_error.empty()) out_error = L"ファイルを書き込めません: " + path;
        return false;
    }
    return true;
}

} // namespace ms

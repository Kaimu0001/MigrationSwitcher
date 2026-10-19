// =============================================================================
// ConfigManager.cpp
// =============================================================================
#include "ConfigManager.h"
#include "FolderScanner.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

ConfigManager::ConfigManager() {}

std::wstring ConfigManager::DefaultIniPath()
{
    return util::JoinPath(util::ModuleDirectory(), L"config.ini");
}

void ConfigManager::SetPath(const std::wstring& ini_path)
{
    ini_path_ = ini_path;
    if (ini_path_.empty()) ini_path_ = DefaultIniPath();
}

static std::wstring GetStr(const std::wstring& ini, const wchar_t* section,
                           const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[2048];
    ::GetPrivateProfileStringW(section, key, def, buf, 2048, ini.c_str());
    return buf;
}

static int GetInt(const std::wstring& ini, const wchar_t* section,
                  const wchar_t* key, int def)
{
    return (int)::GetPrivateProfileIntW(section, key, def, ini.c_str());
}

void ConfigManager::Load(AppConfig& c)
{
    {
        std::wstring ini = ini_path_.empty() ? DefaultIniPath() : ini_path_;
        c.log_source_cpp = ::GetPrivateProfileIntW(L"Log", L"SourceCpp", 0, ini.c_str()) != 0;
        c.gtest_load_enabled = ::GetPrivateProfileIntW(L"GTest", L"LoadEnabled", 1, ini.c_str()) != 0;
    }

    if (ini_path_.empty()) ini_path_ = DefaultIniPath();
    if (!util::PathExists(ini_path_)) return;

    c.last_target_directory = GetStr(ini_path_, L"General", L"LastTargetDirectory", L"");

    std::wstring style = GetStr(ini_path_, L"General", L"DefaultSwitchStyle", L"IFDEF_NEW_FIRST");
    if      (style == L"IFNDEF_OLD_FIRST")  c.default_switch_style = SwitchStyle::IfndefOldFirst;
    else if (style == L"IFDEF_WITH_COMMENT")c.default_switch_style = SwitchStyle::IfdefWithComments;
    else if (style == L"BLOCK_COMMENT")     c.default_switch_style = SwitchStyle::BlockCommentStyle;
    else if (style == L"CUSTOM_TEMPLATE")   c.default_switch_style = SwitchStyle::CustomTemplate;
    else                                    c.default_switch_style = SwitchStyle::IfdefNewFirst;

    c.create_backup  = GetInt(ini_path_, L"General", L"CreateBackup", 1) != 0;
    c.create_html_report = GetInt(ini_path_, L"General", L"CreateHtmlReport", 1) != 0;
    c.create_log     = GetInt(ini_path_, L"General", L"CreateLog", 1) != 0;
    c.open_report_after_execution = GetInt(ini_path_, L"General", L"OpenReportAfterExecution", 0) != 0;
    c.dry_run_only   = GetInt(ini_path_, L"General", L"DryRunOnly", 0) != 0;
    c.auto_indent    = GetInt(ini_path_, L"General", L"AutoIndent", 0) != 0;
    c.warn_comment_match = GetInt(ini_path_, L"General", L"WarnCommentMatch", 1) != 0;
    c.warn_existing_preprocessor = GetInt(ini_path_, L"General", L"WarnExistingPreprocessor", 1) != 0;
    c.preprocessor_scan_lines = GetInt(ini_path_, L"General", L"PreprocessorScanLines", 5);
    if (c.preprocessor_scan_lines < 0) c.preprocessor_scan_lines = 0;
    if (c.preprocessor_scan_lines > 100) c.preprocessor_scan_lines = 100;

    std::wstring hb = GetStr(ini_path_, L"General", L"HitBehavior", L"AUTO_SELECT_SINGLE");
    if      (hb == L"FIRST_ONLY")  c.hit_behavior = HitBehavior::FirstOnly;
    else if (hb == L"ALL_MATCHES") c.hit_behavior = HitBehavior::AllMatches;
    else if (hb == L"MANUAL")      c.hit_behavior = HitBehavior::Manual;
    else                           c.hit_behavior = HitBehavior::AutoSelectSingle;

    std::wstring pm = GetStr(ini_path_, L"General", L"PreviewMode", L"TOP_BOTTOM");
    if      (pm == L"SIMPLE")    c.preview_mode = PreviewMode::Simple;
    if      (pm == L"SIDE_BY_SIDE") c.preview_mode = PreviewMode::SideBySide;
    else if (pm == L"HTML")      c.preview_mode = PreviewMode::Html;
    else if (pm == L"TOP_BOTTOM")c.preview_mode = PreviewMode::TopBottom;

    c.encoding_mode = GetStr(ini_path_, L"General", L"EncodingMode", L"auto");
    c.custom_template = GetStr(ini_path_, L"General", L"CustomTemplate", L"");

    c.default_report_directory = GetStr(ini_path_, L"Report", L"DefaultReportDirectory", L"reports");
    c.default_backup_directory = GetStr(ini_path_, L"Backup", L"DefaultBackupDirectory", L"backups");
    c.winmerge_path = GetStr(ini_path_, L"ExternalTools", L"WinMergePath", L"");

    // ---- 検索条件の拡張（類似一致 / 指定行範囲 / 関数単位 / 出力形式 / Undo 世代）----
    c.search_similar = GetInt(ini_path_, L"Search", L"SimilarEnabled", 0) != 0;
    c.search_similar_threshold = GetInt(ini_path_, L"Search", L"SimilarThreshold", 70);
    if (c.search_similar_threshold < 1)   c.search_similar_threshold = 1;
    if (c.search_similar_threshold > 100) c.search_similar_threshold = 100;
    c.search_line_range = GetInt(ini_path_, L"Search", L"LineRangeEnabled", 0) != 0;
    c.search_line_from = GetInt(ini_path_, L"Search", L"LineFrom", 1);
    if (c.search_line_from < 1) c.search_line_from = 1;
    c.search_line_to = GetInt(ini_path_, L"Search", L"LineTo", 0);
    if (c.search_line_to < 0) c.search_line_to = 0;
    c.search_function = GetInt(ini_path_, L"Search", L"FunctionEnabled", 0) != 0;
    c.search_function_name = GetStr(ini_path_, L"Search", L"FunctionName", L"");
    c.report_markdown = GetInt(ini_path_, L"Search", L"ReportMarkdown", 0) != 0;
    c.report_json     = GetInt(ini_path_, L"Search", L"ReportJson", 0) != 0;
    c.undo_max_generations = GetInt(ini_path_, L"Search", L"UndoMaxGenerations", 10);
    if (c.undo_max_generations < 1)   c.undo_max_generations = 1;
    if (c.undo_max_generations > 100) c.undo_max_generations = 100;
    c.last_export_path = GetStr(ini_path_, L"Search", L"LastExportPath", L"");

    // ---- 追加機能: ログ出力 / レポート作成単位 / GoogleTest ----
    c.log_enabled       = GetInt(ini_path_, L"Log", L"Enabled", 0) != 0;
    c.log_insert_temp   = GetInt(ini_path_, L"Log", L"InsertTempLog", 0) != 0;
    c.log_return_value  = GetInt(ini_path_, L"Log", L"LogReturnValue", 0) != 0;
    c.log_header_name   = GetStr(ini_path_, L"Log", L"HeaderName", L"ms_migration_log.h");
    if (c.log_header_name.empty()) c.log_header_name = L"ms_migration_log.h";

    std::wstring rscope = util::ToUpper(GetStr(ini_path_, L"Report", L"Scope", L"CHANGE"));
    if      (rscope == L"FILE")    c.report_scope = ReportScope::File;
    else if (rscope == L"PROJECT") c.report_scope = ReportScope::Project;
    else                           c.report_scope = ReportScope::Change;

    c.gtest_enabled = GetInt(ini_path_, L"GTest", L"Enabled", 0) != 0;
    c.gtest_load_iterations = GetInt(ini_path_, L"GTest", L"LoadIterations", 1000);
    if (c.gtest_load_iterations < 0) c.gtest_load_iterations = 0;
    if (c.gtest_load_iterations > 100000000) c.gtest_load_iterations = 100000000;
    c.gtest_dir = GetStr(ini_path_, L"GTest", L"OutputDirectory", L"");

    // ---- 17章 複数ファイル一括処理（Phase 4）----
    std::wstring tk = GetStr(ini_path_, L"Batch", L"TargetKind", L"SINGLE_FILE");
    c.target_kind = (util::ToUpper(tk) == L"FOLDER") ? TargetKind::Folder : TargetKind::SingleFile;
    c.last_target_folder = GetStr(ini_path_, L"Batch", L"LastTargetFolder", L"");
    std::wstring exts = GetStr(ini_path_, L"Batch", L"Extensions", L"c,cpp,h");
    c.extension_filter = FolderScanner::SplitExtensions(exts);
    c.include_subdirs = GetInt(ini_path_, L"Batch", L"IncludeSubdirs", 1) != 0;

    // ---- スイッチ定義ヘッダの一元管理 ----
    c.manage_switch_header = GetInt(ini_path_, L"SwitchHeader", L"ManageSwitchHeader", 1) != 0;
    c.add_switch_header_include = GetInt(ini_path_, L"SwitchHeader", L"AddInclude", 1) != 0;
    c.switch_header_name = GetStr(ini_path_, L"SwitchHeader", L"HeaderName", L"migration_switch.h");
    c.switch_header_dir = GetStr(ini_path_, L"SwitchHeader", L"HeaderDirectory", L"");
    c.switch_memo = GetStr(ini_path_, L"SwitchHeader", L"SwitchMemo", L"");
    c.last_switch_header_path = GetStr(ini_path_, L"SwitchHeader", L"LastHeaderPath", L"");

    // ---- AI 連携（仕様書生成）----
    c.ai_provider  = GetInt(ini_path_, L"Ai", L"Provider", 0);
    if (c.ai_provider != 1) c.ai_provider = 0;
    c.ai_openai_key      = GetStr(ini_path_, L"Ai", L"OpenAiApiKey", L"");
    c.ai_openai_endpoint = GetStr(ini_path_, L"Ai", L"OpenAiEndpoint", L"");
    c.ai_openai_model    = GetStr(ini_path_, L"Ai", L"OpenAiModel", L"");
    c.ai_api_key   = GetStr(ini_path_, L"Ai", L"ApiKey", L"");
    c.ai_endpoint  = GetStr(ini_path_, L"Ai", L"Endpoint", L"");
    c.ai_model     = GetStr(ini_path_, L"Ai", L"Model", L"");
    c.ai_task_type = GetStr(ini_path_, L"Ai", L"TaskType", L"super_agent");
    c.ai_timeout_sec = GetInt(ini_path_, L"Ai", L"TimeoutSec", 600);
    if (c.ai_timeout_sec < 30) c.ai_timeout_sec = 30;
    if (c.ai_timeout_sec > 3600) c.ai_timeout_sec = 3600;
    c.ai_open_report = GetInt(ini_path_, L"Ai", L"OpenReportAfterGenerate", 1) != 0;

    // ---- システム情報 ----
    c.sys_name         = GetStr(ini_path_, L"SystemInfo", L"SystemName", L"");
    c.sys_overview     = GetStr(ini_path_, L"SystemInfo", L"SystemOverview", L"");
    c.sys_purpose      = GetStr(ini_path_, L"SystemInfo", L"Purpose", L"");
    c.sys_modification = GetStr(ini_path_, L"SystemInfo", L"ModificationSummary", L"");
    c.sys_extra        = GetStr(ini_path_, L"SystemInfo", L"ExtraInstruction", L"");
    c.last_spec_report_path = GetStr(ini_path_, L"SystemInfo", L"LastSpecReportPath", L"");
}

void ConfigManager::Save(const AppConfig& c)
{
    if (ini_path_.empty()) ini_path_ = DefaultIniPath();

    // 日本語などの非 ANSI 文字を保持するため、新規作成時は UTF-16LE BOM を付ける。
    // （Windows のプロファイル API は BOM の有無で書き込みエンコーディングを決める）
    {
        bool need_bom = true;
        HANDLE hr = ::CreateFileW(ini_path_.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hr != INVALID_HANDLE_VALUE) {
            unsigned char b[2] = { 0, 0 };
            DWORD rd = 0;
            ::ReadFile(hr, b, 2, &rd, nullptr);
            ::CloseHandle(hr);
            // 既存ファイルが ANSI（BOM なし）の場合はそのまま維持する
            need_bom = (rd < 2);
        }
        if (need_bom) {
            HANDLE hw = ::CreateFileW(ini_path_.c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hw != INVALID_HANDLE_VALUE) {
                const unsigned char bom[2] = { 0xFF, 0xFE };
                DWORD wr = 0;
                ::WriteFile(hw, bom, 2, &wr, nullptr);
                ::CloseHandle(hw);
            }
        }
    }

    const wchar_t* style = L"IFDEF_NEW_FIRST";
    switch (c.default_switch_style) {
    case SwitchStyle::IfndefOldFirst:    style = L"IFNDEF_OLD_FIRST"; break;
    case SwitchStyle::IfdefWithComments: style = L"IFDEF_WITH_COMMENT"; break;
    case SwitchStyle::BlockCommentStyle: style = L"BLOCK_COMMENT"; break;
    case SwitchStyle::CustomTemplate:    style = L"CUSTOM_TEMPLATE"; break;
    default: break;
    }

    const wchar_t* hb = L"AUTO_SELECT_SINGLE";
    switch (c.hit_behavior) {
    case HitBehavior::FirstOnly:  hb = L"FIRST_ONLY"; break;
    case HitBehavior::AllMatches: hb = L"ALL_MATCHES"; break;
    case HitBehavior::Manual:     hb = L"MANUAL"; break;
    default: break;
    }

    const wchar_t* pm = L"TOP_BOTTOM";
    switch (c.preview_mode) {
    case PreviewMode::Simple:     pm = L"SIMPLE"; break;
    case PreviewMode::SideBySide: pm = L"SIDE_BY_SIDE"; break;
    case PreviewMode::Html:       pm = L"HTML"; break;
    default: break;
    }

    auto put = [&](const wchar_t* sec, const wchar_t* key, const std::wstring& val) {
        ::WritePrivateProfileStringW(sec, key, val.c_str(), ini_path_.c_str());
    };
    auto puti = [&](const wchar_t* sec, const wchar_t* key, int val) {
        ::WritePrivateProfileStringW(sec, key, std::to_wstring(val).c_str(), ini_path_.c_str());
    };

    put(L"General", L"LastTargetDirectory", c.last_target_directory);
    put(L"General", L"DefaultSwitchStyle", style);
    puti(L"General", L"CreateBackup", c.create_backup ? 1 : 0);
    puti(L"General", L"CreateHtmlReport", c.create_html_report ? 1 : 0);
    puti(L"General", L"CreateLog", c.create_log ? 1 : 0);
    puti(L"General", L"OpenReportAfterExecution", c.open_report_after_execution ? 1 : 0);
    puti(L"General", L"DryRunOnly", c.dry_run_only ? 1 : 0);
    puti(L"General", L"AutoIndent", c.auto_indent ? 1 : 0);
    puti(L"General", L"WarnCommentMatch", c.warn_comment_match ? 1 : 0);
    puti(L"General", L"WarnExistingPreprocessor", c.warn_existing_preprocessor ? 1 : 0);
    puti(L"General", L"PreprocessorScanLines", c.preprocessor_scan_lines);
    put(L"General", L"HitBehavior", hb);
    put(L"General", L"PreviewMode", pm);
    put(L"General", L"EncodingMode", c.encoding_mode);
    put(L"General", L"CustomTemplate", c.custom_template);

    put(L"Report", L"DefaultReportDirectory", c.default_report_directory);
    put(L"Backup", L"DefaultBackupDirectory", c.default_backup_directory);
    put(L"ExternalTools", L"WinMergePath", c.winmerge_path);

    // ---- 検索条件の拡張 ----
    puti(L"Search", L"SimilarEnabled", c.search_similar ? 1 : 0);
    puti(L"Search", L"SimilarThreshold", c.search_similar_threshold);
    puti(L"Search", L"LineRangeEnabled", c.search_line_range ? 1 : 0);
    puti(L"Search", L"LineFrom", c.search_line_from);
    puti(L"Search", L"LineTo", c.search_line_to);
    puti(L"Search", L"FunctionEnabled", c.search_function ? 1 : 0);
    put(L"Search", L"FunctionName", c.search_function_name);
    puti(L"Search", L"ReportMarkdown", c.report_markdown ? 1 : 0);
    puti(L"Search", L"ReportJson", c.report_json ? 1 : 0);
    puti(L"Search", L"UndoMaxGenerations", c.undo_max_generations);
    put(L"Search", L"LastExportPath", c.last_export_path);

    // ---- 17章 複数ファイル一括処理（Phase 4）----
    put(L"Batch", L"TargetKind",
        c.target_kind == TargetKind::Folder ? L"FOLDER" : L"SINGLE_FILE");
    put(L"Batch", L"LastTargetFolder", c.last_target_folder);
    put(L"Batch", L"Extensions", FolderScanner::JoinExtensions(c.extension_filter));
    puti(L"Batch", L"IncludeSubdirs", c.include_subdirs ? 1 : 0);

    // ---- スイッチ定義ヘッダの一元管理 ----
    puti(L"SwitchHeader", L"ManageSwitchHeader", c.manage_switch_header ? 1 : 0);
    puti(L"SwitchHeader", L"AddInclude", c.add_switch_header_include ? 1 : 0);
    put(L"SwitchHeader", L"HeaderName", c.switch_header_name);
    put(L"SwitchHeader", L"HeaderDirectory", c.switch_header_dir);
    put(L"SwitchHeader", L"SwitchMemo", c.switch_memo);
    put(L"SwitchHeader", L"LastHeaderPath", c.last_switch_header_path);

    // ---- AI 連携（仕様書生成）----
    puti(L"Ai", L"Provider", c.ai_provider);
    put(L"Ai", L"OpenAiApiKey", c.ai_openai_key);
    put(L"Ai", L"OpenAiEndpoint", c.ai_openai_endpoint);
    put(L"Ai", L"OpenAiModel", c.ai_openai_model);
    put(L"Ai", L"ApiKey", c.ai_api_key);
    put(L"Ai", L"Endpoint", c.ai_endpoint);
    put(L"Ai", L"Model", c.ai_model);
    put(L"Ai", L"TaskType", c.ai_task_type.empty() ? L"super_agent" : c.ai_task_type);
    puti(L"Ai", L"TimeoutSec", c.ai_timeout_sec);
    puti(L"Ai", L"OpenReportAfterGenerate", c.ai_open_report ? 1 : 0);

    // ---- システム情報 ----
    put(L"SystemInfo", L"SystemName", c.sys_name);
    put(L"SystemInfo", L"SystemOverview", c.sys_overview);
    put(L"SystemInfo", L"Purpose", c.sys_purpose);
    put(L"SystemInfo", L"ModificationSummary", c.sys_modification);
    put(L"SystemInfo", L"ExtraInstruction", c.sys_extra);
    put(L"SystemInfo", L"LastSpecReportPath", c.last_spec_report_path);
}

// -----------------------------------------------------------------------------
// 追加機能の設定を書き出す（既存の Save から呼ばれる）
// -----------------------------------------------------------------------------
void ConfigManager::SaveExtras(const AppConfig& c)
{
    if (ini_path_.empty()) ini_path_ = DefaultIniPath();
    const wchar_t* ini = ini_path_.c_str();

    ::WritePrivateProfileStringW(L"Log", L"Enabled",
        c.log_enabled ? L"1" : L"0", ini);
    ::WritePrivateProfileStringW(L"Log", L"InsertTempLog",
        c.log_insert_temp ? L"1" : L"0", ini);
    ::WritePrivateProfileStringW(L"Log", L"LogReturnValue",
        c.log_return_value ? L"1" : L"0", ini);
    ::WritePrivateProfileStringW(L"Log", L"HeaderName", c.log_header_name.c_str(), ini);

    const wchar_t* rs = L"CHANGE";
    if (c.report_scope == ReportScope::File)         rs = L"FILE";
    else if (c.report_scope == ReportScope::Project) rs = L"PROJECT";
    ::WritePrivateProfileStringW(L"Report", L"Scope", rs, ini);

    ::WritePrivateProfileStringW(L"GTest", L"Enabled",
        c.gtest_enabled ? L"1" : L"0", ini);
    ::WritePrivateProfileStringW(L"GTest", L"LoadIterations",
        std::to_wstring(c.gtest_load_iterations).c_str(), ini);
    ::WritePrivateProfileStringW(L"GTest", L"LoadEnabled",
        c.gtest_load_enabled ? L"1" : L"0", ini);
    ::WritePrivateProfileStringW(L"GTest", L"OutputDirectory", c.gtest_dir.c_str(), ini);
    ::WritePrivateProfileStringW(L"Log", L"SourceCpp",
        c.log_source_cpp ? L"1" : L"0", ini);
}

} // namespace ms

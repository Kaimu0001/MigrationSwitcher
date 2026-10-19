// =============================================================================
// LogRuntimeGenerator.h
// 生成したソースへ挿入するログ関数（ms_migration_log.h / .c / .cpp）を生成する
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

struct LogRuntimeOptions {
    std::wstring header_name = L"ms_migration_log.h";
    std::wstring source_name = L"ms_migration_log.c";
    bool         source_is_cpp = false;    // true なら .cpp として出力
    bool         include_source = true;
    bool         default_enabled = true;
};

class LogRuntimeGenerator {
public:
    static std::string HeaderText(const LogRuntimeOptions& opt);
    static std::string SourceText(const LogRuntimeOptions& opt);

    static bool WriteTo(const std::wstring& dir,
                        const LogRuntimeOptions& opt,
                        std::vector<std::wstring>& out_files,
                        std::wstring& out_error);

    static std::wstring UsageText();
};

} // namespace ms

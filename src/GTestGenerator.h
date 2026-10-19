// =============================================================================
// GTestGenerator.h
// 追加機能: 関数単位の GoogleTest 生成と自動実行（負荷試験を含む）
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

struct GTestTarget {
    std::wstring file_path;
    std::wstring func_name;
    std::wstring return_type;
    std::vector<std::wstring> param_decls;   // 例: "int item"
    std::vector<std::wstring> param_names;   // 例: "item"
    bool         is_c        = true;
    bool         callable    = true;         // 引数を生成して呼び出せるか
    std::wstring note;                       // 呼べない理由
};

struct GTestOptions {
    std::wstring out_dir;                    // 既定: <root>/tests/gtest
    std::wstring project_name = L"MigrationGTest";
    int          load_iterations = 1000;
    bool         generate_load = true;
    bool         assume_c = true;
    std::wstring switch_name;               // 外部トグル用（MS_SWITCH_ON）

    GTestOptions() {}
};

struct GTestRunResult {
    bool         generated = false;
    bool         built     = false;
    bool         ran       = false;
    int          tests     = 0;
    int          failures  = 0;
    int          disabled  = 0;
    double       seconds   = 0.0;
    std::wstring build_log;
    std::wstring run_log;
    std::wstring exe_path;
    std::wstring json_path;
    std::wstring summary;
};

class GTestGenerator {
public:
    // 一致箇所を含む関数を対象にする（重複は除去）
    static std::vector<GTestTarget> CollectTargets(const std::wstring& text,
                                                   const std::wstring& file_path,
                                                   const std::vector<MatchResult>& matches);

    // テストプロジェクトを生成する
    static bool WriteProject(const std::wstring& root_dir,
                             const std::vector<GTestTarget>& targets,
                             const GTestOptions& opt,
                             std::vector<std::wstring>& out_files,
                             std::wstring& out_error);

    // CMake でビルドして実行し、結果を JSON から集計する
    static bool BuildAndRun(const std::wstring& root_dir,
                            const GTestOptions& opt,
                            GTestRunResult& out,
                            std::wstring& out_error);

    static std::wstring DefaultOutDir(const std::wstring& root_dir);

    // 関数シグネチャの解析（テスト用に公開）
    static bool ParseSignature(const std::wstring& text,
                               const std::wstring& func_name,
                               GTestTarget& out);
};

} // namespace ms

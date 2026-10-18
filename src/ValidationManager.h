// =============================================================================
// ValidationManager.h
// 設計書 6.2.3 スイッチ名バリデーション / 6.3 旧コード / 6.4 新コード / 14章 実行前チェック
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

struct ValidationResult {
    bool ok = true;
    std::wstring error;                    // 致命的エラー（処理中断）
    std::vector<std::wstring> warnings;    // 警告（続行可）

    void AddError(const std::wstring& m) { ok = false; error = m; }
    void AddWarning(const std::wstring& m) { warnings.push_back(m); }
};

class ValidationManager {
public:
    // 6.2.3 コンパイルスイッチ名
    static ValidationResult ValidateSwitchName(const std::wstring& name);

    // 6.3.2 差し替え対象コード
    static ValidationResult ValidateOldCode(const std::wstring& old_code);

    // 6.4.2 新コード
    static ValidationResult ValidateNewCode(const std::wstring& old_code,
                                            const std::wstring& new_code);

    // 6.1.4 / 19.2 対象ファイル
    static ValidationResult ValidateTargetFile(const std::wstring& path,
                                               const FileInfo& info);

    // 14.1 実行前チェック（個別）
    static bool IsReservedWord(const std::wstring& name);
    static bool IsValidSymbolChars(const std::wstring& name, int& bad_index, wchar_t& bad_char);

    // 16章 / 15章 警告メッセージ生成
    static std::wstring BuildWarningSummary(const std::vector<std::wstring>& warnings);
};

} // namespace ms

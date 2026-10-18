// =============================================================================
// BackupManager.h
// 設計書 6.9「バックアップ機能」/ 7.2.8 BackupManager
// =============================================================================
#pragma once

#include <string>

#include "Models.h"

namespace ms {

class BackupManager {
public:
    // 6.9.2 対象ファイル名.yyyymmdd_hhmmss.bak
    // 6.9.3 backup_dir が空の場合は対象ファイルと同一フォルダ
    static bool CreateBackup(const std::wstring& target_file,
                             const std::wstring& backup_dir,
                             std::wstring& out_backup_path,
                             std::wstring& out_error);

    // バックアップ先が書き込み可能かを事前確認する（14.1 No.10）
    static bool CanCreateBackup(const std::wstring& target_file,
                                const std::wstring& backup_dir,
                                std::wstring& out_error);

    static std::wstring BuildBackupFileName(const std::wstring& target_file);
};

} // namespace ms

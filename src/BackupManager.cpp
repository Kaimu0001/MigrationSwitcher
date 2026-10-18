// =============================================================================
// BackupManager.cpp
// =============================================================================
#include "BackupManager.h"
#include <cstdio>
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

std::wstring BackupManager::BuildBackupFileName(const std::wstring& target_file)
{
    return util::GetFileNameOf(target_file) + L"." +
           util::TimestampFileName() + L".bak";
}

bool BackupManager::CreateBackup(const std::wstring& target_file,
                                 const std::wstring& backup_dir,
                                 std::wstring& out_backup_path,
                                 std::wstring& out_error)
{
    out_backup_path.clear();
    out_error.clear();

    if (target_file.empty() || !util::PathExists(target_file)) {
        out_error = L"バックアップ対象のファイルが存在しません。";
        return false;
    }

    std::wstring dir = backup_dir.empty()
        ? util::GetDirectoryOf(target_file)
        : util::MakeAbsolute(backup_dir);
    if (dir.empty()) dir = util::ModuleDirectory();

    if (!util::EnsureDirectory(dir)) {
        out_error = L"バックアップ先フォルダを作成できません: " + dir;
        return false;
    }

    std::wstring path = util::JoinPath(dir, BuildBackupFileName(target_file));
    int n = 0;
    while (util::PathExists(path)) {
        wchar_t suffix[16];
        swprintf(suffix, 16, L"_%d", ++n);
        path = util::JoinPath(dir, util::GetFileNameOf(target_file) + L"." +
                                    util::TimestampFileName() + suffix + L".bak");
    }

    std::wstring err;
    if (!FileManager::CopyFileTo(target_file, path, err)) {
        out_error = L"バックアップの作成に失敗しました: " + err;
        return false;
    }

    // コピー後のサイズ検証
    WIN32_FILE_ATTRIBUTE_DATA sa, da;
    if (::GetFileAttributesExW(target_file.c_str(), GetFileExInfoStandard, &sa) &&
        ::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &da)) {
        ULARGE_INTEGER us, ud;
        us.LowPart = sa.nFileSizeLow;  us.HighPart = sa.nFileSizeHigh;
        ud.LowPart = da.nFileSizeLow;  ud.HighPart = da.nFileSizeHigh;
        if (us.QuadPart != ud.QuadPart) {
            ::DeleteFileW(path.c_str());
            out_error = L"バックアップのサイズが一致しません。実行を中断します。";
            return false;
        }
    }

    out_backup_path = path;
    return true;
}

bool BackupManager::CanCreateBackup(const std::wstring& target_file,
                                    const std::wstring& backup_dir,
                                    std::wstring& out_error)
{
    out_error.clear();
    std::wstring dir = backup_dir.empty()
        ? util::GetDirectoryOf(target_file)
        : util::MakeAbsolute(backup_dir);
    if (dir.empty()) {
        out_error = L"バックアップ先が特定できません。";
        return false;
    }
    if (!util::EnsureDirectory(dir)) {
        out_error = L"バックアップ先フォルダを作成できません: " + dir;
        return false;
    }
    std::wstring probe = util::JoinPath(dir, L"__ms_backup_probe.tmp");
    HANDLE h = ::CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        out_error = L"バックアップ先に書き込めません: " + dir;
        return false;
    }
    ::CloseHandle(h);
    ::DeleteFileW(probe.c_str());
    return true;
}

} // namespace ms

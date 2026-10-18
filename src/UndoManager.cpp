// =============================================================================
// UndoManager.cpp
// =============================================================================
#include "UndoManager.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

UndoManager::UndoManager() {}

void UndoManager::SetRecord(const UndoRecord& record)
{
    records_.clear();
    UndoRecord r = record;
    r.valid = !r.backup_file_path.empty() && util::PathExists(r.backup_file_path);
    records_.push_back(r);
}

void UndoManager::SetRecords(const std::vector<UndoRecord>& records)
{
    records_ = records;
    for (size_t i = 0; i < records_.size(); ++i) {
        records_[i].valid = records_[i].remove_on_undo
            ? true
            : (!records_[i].backup_file_path.empty() &&
               util::PathExists(records_[i].backup_file_path));
    }
}

void UndoManager::Clear()
{
    records_.clear();
}

const UndoRecord& UndoManager::Record() const
{
    static const UndoRecord empty;
    if (records_.empty()) return empty;
    return records_[0];
}

bool UndoManager::CanUndo() const
{
    if (records_.empty()) return false;
    for (size_t i = 0; i < records_.size(); ++i) {
        if (!records_[i].valid) return false;
        if (records_[i].target_file_path.empty()) return false;
        if (records_[i].remove_on_undo) continue;
        if (!util::PathExists(records_[i].backup_file_path)) return false;
    }
    return true;
}

bool UndoManager::ValidateForUndo(std::wstring& out_error) const
{
    out_error.clear();
    if (records_.empty()) {
        out_error = L"元に戻せる操作がありません。";
        return false;
    }
    for (size_t i = 0; i < records_.size(); ++i) {
        const UndoRecord& r = records_[i];
        if (!r.valid) {
            out_error = L"元に戻せない記録が含まれています（バックアップが無効）。";
            return false;
        }
        if (!r.remove_on_undo && !util::PathExists(r.backup_file_path)) {
            out_error = L"バックアップファイルが見つかりません: " + r.backup_file_path;
            return false;
        }
        if (!util::PathExists(r.target_file_path)) {
            out_error = L"対象ファイルが見つかりません: " + r.target_file_path;
            return false;
        }
    }
    return true;
}

bool UndoManager::Undo(std::wstring& out_error, std::vector<std::wstring>& out_sidecars)
{
    out_error.clear();
    out_sidecars.clear();

    if (!ValidateForUndo(out_error)) return false;

    // 6.10 手順4: 現在ファイルを退避（全ファイル分）
    std::vector<std::wstring> sidecars;
    for (size_t i = 0; i < records_.size(); ++i) {
        const UndoRecord& r = records_[i];
        std::wstring dir = util::GetDirectoryOf(r.target_file_path);
        std::wstring sidecar = util::JoinPath(
            dir, util::GetFileNameOf(r.target_file_path) + L"." +
                 util::TimestampFileName() + L"_" + std::to_wstring((int)i + 1) + L".undobak");
        std::wstring err;
        if (!FileManager::CopyFileTo(r.target_file_path, sidecar, err)) {
            out_error = L"現在のファイルを退避できませんでした: " +
                        r.target_file_path + L"（" + err + L"）";
            return false;
        }
        sidecars.push_back(sidecar);
    }
    out_sidecars = sidecars;

    // 6.10 手順5: バックアップを対象ファイルへコピー（全ファイル分）
    for (size_t i = 0; i < records_.size(); ++i) {
        const UndoRecord& r = records_[i];
        if (r.remove_on_undo) {
            // 新規作成されたファイルは削除して「無かった状態」へ戻す
            if (util::PathExists(r.target_file_path)) {
                if (!FileManager::RemoveFile(r.target_file_path)) {
                    out_error = L"新規作成されたファイルを削除できませんでした: " +
                                r.target_file_path;
                    return false;
                }
            }
            continue;
        }
        std::wstring err;
        if (!FileManager::CopyFileTo(r.backup_file_path, r.target_file_path, err)) {
            out_error = L"バックアップからの復元に失敗しました: " +
                        r.target_file_path + L"（" + err + L"）";
            return false;
        }
    }

    // 6.10 手順6: 復元成功確認（サイズ一致）
    for (size_t i = 0; i < records_.size(); ++i) {
        const UndoRecord& r = records_[i];
        if (r.remove_on_undo) continue;
        WIN32_FILE_ATTRIBUTE_DATA sa, da;
        if (::GetFileAttributesExW(r.backup_file_path.c_str(), GetFileExInfoStandard, &sa) &&
            ::GetFileAttributesExW(r.target_file_path.c_str(), GetFileExInfoStandard, &da)) {
            ULARGE_INTEGER us, ud;
            us.LowPart = sa.nFileSizeLow; us.HighPart = sa.nFileSizeHigh;
            ud.LowPart = da.nFileSizeLow; ud.HighPart = da.nFileSizeHigh;
            if (us.QuadPart != ud.QuadPart) {
                out_error = L"復元後のサイズが一致しません。手動で確認してください: " +
                            r.target_file_path;
                return false;
            }
        }
    }

    records_.clear();   // 復元後は Undo 不可
    return true;
}

} // namespace ms

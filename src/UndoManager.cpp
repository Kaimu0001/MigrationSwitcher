// =============================================================================
// UndoManager.cpp
// 複数世代 Undo 対応。history_ の末尾が最新世代。
// =============================================================================
#include "UndoManager.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

static const std::vector<UndoRecord> kEmptyRecords;

UndoManager::UndoManager() : max_generations_(10) {}

const std::vector<UndoRecord>& UndoManager::Top() const
{
    if (history_.empty()) return kEmptyRecords;
    return history_[history_.size() - 1];
}

void UndoManager::SetMaxGenerations(int n)
{
    if (n < 1) n = 1;
    if (n > 100) n = 100;
    max_generations_ = n;
    TrimToMax();
}

void UndoManager::TrimToMax()
{
    while ((int)history_.size() > max_generations_)
        history_.erase(history_.begin());
}

void UndoManager::SetRecord(const UndoRecord& record)
{
    std::vector<UndoRecord> g;
    UndoRecord r = record;
    r.valid = r.remove_on_undo
        ? true
        : (!r.backup_file_path.empty() && util::PathExists(r.backup_file_path));
    g.push_back(r);
    history_.push_back(g);
    TrimToMax();
}

void UndoManager::SetRecords(const std::vector<UndoRecord>& records)
{
    std::vector<UndoRecord> g = records;
    for (size_t i = 0; i < g.size(); ++i) {
        g[i].valid = g[i].remove_on_undo
            ? true
            : (!g[i].backup_file_path.empty() && util::PathExists(g[i].backup_file_path));
    }
    history_.push_back(g);
    TrimToMax();
}

void UndoManager::Clear()
{
    history_.clear();
}

const UndoRecord& UndoManager::Record() const
{
    static const UndoRecord empty;
    const std::vector<UndoRecord>& t = Top();
    if (t.empty()) return empty;
    return t[0];
}

const std::vector<UndoRecord>& UndoManager::Records() const
{
    return Top();
}

int UndoManager::FileCount() const
{
    return (int)Top().size();
}

bool UndoManager::CanUndo() const
{
    const std::vector<UndoRecord>& t = Top();
    if (t.empty()) return false;
    for (size_t i = 0; i < t.size(); ++i) {
        if (!t[i].valid) return false;
        if (t[i].target_file_path.empty()) return false;
        if (t[i].remove_on_undo) continue;
        if (!util::PathExists(t[i].backup_file_path)) return false;
    }
    return true;
}

bool UndoManager::ValidateForUndo(std::wstring& out_error) const
{
    out_error.clear();
    const std::vector<UndoRecord>& t = Top();
    if (t.empty()) {
        out_error = L"元に戻せる操作がありません。";
        return false;
    }
    for (size_t i = 0; i < t.size(); ++i) {
        const UndoRecord& r = t[i];
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

std::wstring UndoManager::GenerationLabel(int index_from_top) const
{
    if (index_from_top < 0 || index_from_top >= (int)history_.size())
        return std::wstring();
    const std::vector<UndoRecord>& g = history_[history_.size() - 1 - (size_t)index_from_top];
    if (g.empty()) return L"(空の世代)";

    std::wstring when = g[0].executed_at;
    std::wstring op = g[0].operation_id;
    std::wstring target = util::GetFileNameOf(g[0].target_file_path);

    std::wstring s = L"";
    if (!when.empty())   s += when + L" ";
    if (!op.empty())     s += L"[" + op + L"] ";
    if (g.size() > 1)    s += std::to_wstring((int)g.size()) + L" ファイル（一括処理）";
    else                 s += target;
    return s;
}

std::vector<std::wstring> UndoManager::HistoryLabels() const
{
    std::vector<std::wstring> out;
    for (int i = 0; i < (int)history_.size(); ++i)
        out.push_back(GenerationLabel(i));
    return out;
}

bool UndoManager::Undo(std::wstring& out_error, std::vector<std::wstring>& out_sidecars)
{
    out_error.clear();
    out_sidecars.clear();

    if (!ValidateForUndo(out_error)) return false;

    const std::vector<UndoRecord> gen = Top();   // 対象世代をコピー

    // 6.10 手順4: 現在ファイルを退避（全ファイル分）
    std::vector<std::wstring> sidecars;
    for (size_t i = 0; i < gen.size(); ++i) {
        const UndoRecord& r = gen[i];
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
    for (size_t i = 0; i < gen.size(); ++i) {
        const UndoRecord& r = gen[i];
        if (r.remove_on_undo) {
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
    for (size_t i = 0; i < gen.size(); ++i) {
        const UndoRecord& r = gen[i];
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

    // 直近 1 世代だけを取り除く（以前の世代は引き続き元に戻せる）
    history_.pop_back();
    return true;
}

} // namespace ms

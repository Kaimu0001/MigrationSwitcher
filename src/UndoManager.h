// =============================================================================
// UndoManager.h
// 設計書 6.10「Undo機能」/ 8.5 Undo履歴 / 7.2.9 UndoManager
// 1 操作 = 1 世代として履歴に積み、複数世代さかのぼって元に戻せる。
// 単一ファイル操作も一括処理も同じ扱い（一括は 1 世代に複数ファイル）。
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

class UndoManager {
public:
    UndoManager();

    // 単一ファイル操作の記録（1 世代として積む）
    void SetRecord(const UndoRecord& record);
    // 一括操作の記録（1 操作 = 複数ファイルを 1 世代として積む）
    void SetRecords(const std::vector<UndoRecord>& records);

    void Clear();
    bool CanUndo() const;

    // 直近世代の代表レコード（存在しない場合は valid == false）
    const UndoRecord& Record() const;
    const std::vector<UndoRecord>& Records() const;

    int FileCount() const;

    // バックアップから対象ファイルへ復元する（直近 1 世代を戻す）
    // out_sidecars: 復元前に退避した現在内容のパス一覧
    bool Undo(std::wstring& out_error, std::vector<std::wstring>& out_sidecars);

    // 対象ファイルが現在も存在し、変更可能かを確認する
    bool ValidateForUndo(std::wstring& out_error) const;

    // ---- 複数世代 Undo ----
    void SetMaxGenerations(int n);
    int  MaxGenerations() const { return max_generations_; }
    int  HistorySize() const { return (int)history_.size(); }
    // 新しい順に 0 が最新世代
    std::wstring GenerationLabel(int index_from_top) const;
    std::vector<std::wstring> HistoryLabels() const;

private:
    std::vector<std::vector<UndoRecord> > history_;   // 末尾が最新世代
    int max_generations_;

    const std::vector<UndoRecord>& Top() const;
    void TrimToMax();
};

} // namespace ms

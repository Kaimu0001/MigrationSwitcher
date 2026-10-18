// =============================================================================
// UndoManager.h
// 設計書 6.10「Undo機能」/ 8.5 Undo履歴 / 7.2.9 UndoManager
// 単一ファイルは直前 1 回（6.10.3）。
// 一括処理（17章）では 1 操作 = 複数ファイルのため、操作単位でまとめて復元する。
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

class UndoManager {
public:
    UndoManager();

    // 単一ファイル操作の記録（直前 1 回のみ）
    void SetRecord(const UndoRecord& record);
    // 一括操作の記録（1 操作 = 複数ファイル）
    void SetRecords(const std::vector<UndoRecord>& records);

    void Clear();
    bool CanUndo() const;

    // 直近操作の代表レコード（存在しない場合は valid == false）
    const UndoRecord& Record() const;
    const std::vector<UndoRecord>& Records() const { return records_; }

    int FileCount() const { return (int)records_.size(); }

    // バックアップから対象ファイルへ復元する（操作内の全ファイルを戻す）
    // out_sidecars: 復元前に退避した現在内容のパス一覧
    bool Undo(std::wstring& out_error, std::vector<std::wstring>& out_sidecars);

    // 対象ファイルが現在も存在し、変更可能かを確認する
    bool ValidateForUndo(std::wstring& out_error) const;

private:
    std::vector<UndoRecord> records_;
};

} // namespace ms

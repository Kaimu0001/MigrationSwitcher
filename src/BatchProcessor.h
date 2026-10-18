// =============================================================================
// BatchProcessor.h
// 設計書 17章「複数ファイル対応設計」（Phase 4）
// 17.2.2 処理仕様 手順1～8 を UI 非依存で実装する
// =============================================================================
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Models.h"

namespace ms {

// 進捗通知（done: 処理済み件数 / total: 総件数 / current: 現在のファイル）
typedef std::function<void(int done, int total, const std::wstring& current)> BatchProgressFn;

struct BatchSearchResult {
    std::vector<BatchFileEntry> entries;      // ヒットしたファイル（text を保持）
    std::vector<std::wstring>   all_files;    // 走査で抽出された全ファイル
    std::vector<std::wstring>   errors;       // 読み込み失敗など
    std::vector<std::wstring>   skipped;      // サイズ過大などで除外
    std::vector<MatchResult>    matches;      // 集約されたヒット一覧
    int  scanned_count = 0;
    int  hit_file_count = 0;
    int  total_matches = 0;
    bool truncated = false;
};

class BatchProcessor {
public:
    // 手順1-4: 走査 → 拡張子フィルタ → 各ファイルを検索 → ヒット一覧へ集約
    static bool SearchFolder(const std::wstring& folder,
                             const std::vector<std::wstring>& extensions,
                             bool include_subdirs,
                             const std::wstring& pattern,
                             const std::wstring& encoding_mode,
                             const BatchProgressFn& progress,
                             BatchSearchResult& out,
                             std::wstring& out_error);

    // 手順7-8: 一括バックアップと一括変換
    //   blocks は matches と同じ並び（matches[i] に対応する置換ブロック。未選択要素は空で可）
    //   ctx.execution_mode == DryRun の場合はバックアップも書き換えも行わない
    static bool ExecuteBatch(const OperationContext& ctx,
                             const std::wstring& encoding_mode,
                             std::vector<BatchFileEntry>& entries,
                             std::vector<MatchResult>& matches,
                             const std::vector<std::wstring>& blocks,
                             const std::wstring& backup_dir,
                             const BatchProgressFn& progress,
                             BatchExecuteResult& out,
                             std::vector<UndoRecord>& out_undo,
                             std::wstring& out_error);
};

} // namespace ms

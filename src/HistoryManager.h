// =============================================================================
// HistoryManager.h
// 実行ごとの変更前 / 変更後スナップショットを undobak フォルダへ保存し、
// 履歴タブ・比較（WinMerge / レポート）・特定変更だけの復元に使う。
//
//   <base>\undobak\
//     runs\run_20261019_161800_001\
//       meta.ini          （[Run] Time / Operation / Mode / Files / Replaced）
//       files.tsv         （idx <TAB> path <TAB> before <TAB> after）
//       files\0001_name.before / .after
//     sidecars\           （Undo 時に退避した現在内容）
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

struct HistoryFileEntry {
    std::wstring path;         // 元のファイルパス
    std::wstring before;       // 変更前スナップショット（実ファイルパス）
    std::wstring after;        // 変更後スナップショット（実ファイルパス）
    bool         has_before = false;
    bool         has_after  = false;
};

struct HistoryRunInfo {
    std::wstring id;           // run_20261019_161800_001
    std::wstring time;         // 2026-10-19 16:18:00
    std::wstring operation;    // 操作ID
    std::wstring mode;         // Real / DryRun / Batch / Undo
    int          files = 0;
    int          replaced = 0;
};

class HistoryManager {
public:
    // <base>\undobak
    static std::wstring RootFor(const std::wstring& base_dir);

    // スナップショットを記録する（src の before/after は実ファイルパス）
    static bool Record(const std::wstring& base_dir,
                       const std::wstring& operation,
                       const std::wstring& mode,
                       int replaced,
                       const std::vector<HistoryFileEntry>& src,
                       HistoryRunInfo& out_run,
                       std::wstring& out_error);

    // 新しい順に一覧を返す
    static std::vector<HistoryRunInfo> ListRuns(const std::wstring& base_dir);
    static std::vector<HistoryFileEntry> ListFiles(const std::wstring& base_dir,
                                                   const std::wstring& run_id);

    static bool LoadText(const std::wstring& path, std::wstring& out, std::wstring& out_error);

    // 2 つの実行の「変更前 → 変更後」の差分テキストを作る
    static std::wstring BuildDiffText(const std::wstring& before,
                                      const std::wstring& after,
                                      int max_lines);

    // サイドカー（Undo 時の退避）の保存先
    static std::wstring SidecarDirectory(const std::wstring& base_dir);

    static std::wstring RunLabel(const HistoryRunInfo& r);
};

} // namespace ms

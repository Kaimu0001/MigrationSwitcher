// =============================================================================
// BatchProcessor.cpp
// =============================================================================
#include "BatchProcessor.h"
#include "BackupManager.h"
#include "FileManager.h"
#include "FolderScanner.h"
#include "PreviewGenerator.h"
#include "SearchEngine.h"
#include "SwitchHeaderManager.h"
#include "Utils.h"

#include <algorithm>
#include <map>

namespace ms {

// -----------------------------------------------------------------------------
// 手順1-4: 走査・拡張子フィルタ・検索・集約
// -----------------------------------------------------------------------------
bool BatchProcessor::SearchFolder(const std::wstring& folder,
                                  const std::vector<std::wstring>& extensions,
                                  bool include_subdirs,
                                  const std::wstring& pattern,
                                  const std::wstring& encoding_mode,
                                  const BatchProgressFn& progress,
                                  BatchSearchResult& out,
                                  std::wstring& out_error)
{
    out = BatchSearchResult();
    out_error.clear();

    // 手順1-2: フォルダ配下を走査し、拡張子フィルタで対象抽出
    ScanResult scan;
    FolderScanner::Scan(folder, extensions, include_subdirs, scan);
    out.errors    = scan.errors;
    out.skipped   = scan.skipped;
    out.truncated = scan.truncated;
    out.all_files = scan.files;
    out.scanned_count = (int)scan.files.size();

    if (scan.files.empty()) {
        out_error = L"対象フォルダに条件に一致するファイルが見つかりませんでした。";
        return false;
    }

    // 手順3-4: 各ファイルを検索し、ヒット一覧へ集約
    int total = (int)scan.files.size();
    int done = 0;
    int global_id = 0;

    for (size_t i = 0; i < scan.files.size(); ++i) {
        const std::wstring& path = scan.files[i];
        if (progress) progress(done, total, path);

        std::wstring text;
        FileInfo info;
        if (!FileManager::ReadText(path, encoding_mode, text, info)) {
            out.errors.push_back(L"読み取りに失敗しました: " + path);
            ++done;
            continue;
        }
        if (text.empty()) { ++done; continue; }

        std::vector<MatchResult> found = SearchEngine::Search(text, pattern, path);
        if (!found.empty()) {
            for (size_t k = 0; k < found.size(); ++k)
                found[k].match_id = ++global_id;

            BatchFileEntry e;
            e.path = path;
            e.text = text;
            e.info = info;
            e.hit_count = (int)found.size();
            e.selected_count = (int)found.size();
            e.selected = true;
            e.status = L"待機";
            out.entries.push_back(e);

            out.matches.insert(out.matches.end(), found.begin(), found.end());
            ++out.hit_file_count;
            out.total_matches += (int)found.size();
        }
        ++done;
    }

    if (progress) progress(total, total, std::wstring());
    return true;
}

// -----------------------------------------------------------------------------
// 手順7-8: 一括バックアップと一括変換
// -----------------------------------------------------------------------------
bool BatchProcessor::ExecuteBatch(const OperationContext& ctx,
                                  const std::wstring& encoding_mode,
                                  std::vector<BatchFileEntry>& entries,
                                  std::vector<MatchResult>& matches,
                                  const std::vector<std::wstring>& blocks,
                                  const std::wstring& backup_dir,
                                  const BatchProgressFn& progress,
                                  BatchExecuteResult& out,
                                  std::vector<UndoRecord>& out_undo,
                                  std::wstring& out_error)
{
    out = BatchExecuteResult();
    out_undo.clear();
    out_error.clear();

    const bool dry_run = (ctx.execution_mode == ExecutionMode::DryRun);

    out.operation_id = ctx.operation_id;
    out.executed_at  = ctx.started_at;
    out.folder       = ctx.target_folder;
    out.status       = L"Success";

    // ファイル単位に一致箇所をまとめる
    std::map<std::wstring, std::vector<size_t> > by_file;
    for (size_t i = 0; i < matches.size(); ++i)
        by_file[matches[i].file_path].push_back(i);

    int total = (int)entries.size();
    int done = 0;

    for (size_t ei = 0; ei < entries.size(); ++ei) {
        BatchFileEntry& e = entries[ei];
        if (progress) progress(done, total, e.path);

        std::map<std::wstring, std::vector<size_t> >::iterator it = by_file.find(e.path);
        if (it == by_file.end()) { ++done; continue; }

        // このファイルで選択されている一致箇所を抽出
        std::vector<size_t> idx;
        for (size_t k = 0; k < it->second.size(); ++k) {
            size_t mi = it->second[k];
            if (matches[mi].selected) idx.push_back(mi);
        }

        FileReplaceResult fr;
        fr.file_path = e.path;

        if (!e.selected || idx.empty()) {
            e.status = L"対象外";
            fr.status = L"Skipped";
            fr.error  = L"対象として選択されていません。";
            out.files.push_back(fr);
            ++out.skipped_count;
            ++done;
            continue;
        }

        // 手順5相当: 検索結果の再検証（本実行のみ。検索後にファイルが変更されていないか）
        if (!dry_run) {
            std::wstring current;
            FileInfo cur_info;
            if (!FileManager::ReadText(e.path, encoding_mode, current, cur_info)) {
                e.status = L"失敗";
                e.error  = L"読み取りに失敗しました。";
                fr.status = L"Failed";
                fr.error  = e.error;
                out.files.push_back(fr);
                ++out.error_count;
                ++done;
                continue;
            }
            if (current != e.text) {
                e.status = L"変更検出";
                e.error  = L"検索後にファイルが変更されています。再検索してください。";
                fr.status = L"Skipped";
                fr.error  = e.error;
                out.files.push_back(fr);
                ++out.skipped_count;
                ++done;
                continue;
            }
        }

        // 変更後内容の生成（一致箇所ごとのブロックを適用）
        std::vector<MatchResult> file_matches;
        std::vector<std::wstring> file_blocks;
        for (size_t k = 0; k < idx.size(); ++k) {
            file_matches.push_back(matches[idx[k]]);
            file_blocks.push_back(blocks[idx[k]]);
        }
        std::wstring after =
            PreviewGenerator::BuildAfterTextEach(e.text, file_matches, file_blocks);

        // スイッチ定義ヘッダの #include を追加する
        if (ctx.add_switch_header_include && !ctx.switch_header_path.empty()) {
            std::wstring inc =
                SwitchHeaderManager::MakeIncludePath(e.path, ctx.switch_header_path);
            bool added = false;
            int line = 0;
            std::wstring ierr;
            if (!SwitchHeaderManager::EnsureInclude(after, inc, added, line, ierr)) {
                e.status = L"失敗";
                e.error  = ierr;
                fr.status = L"Failed";
                fr.error  = ierr;
                out.files.push_back(fr);
                ++out.error_count;
                ++done;
                continue;
            }
            e.include_added = added;
            e.include_line = line;
        }

        // ---- ドライラン: バックアップも書き換えも行わない（6.11）----
        if (dry_run) {
            e.status = L"ドライラン";
            fr.status = L"DryRun";
            fr.replaced_count = (int)idx.size();
            for (size_t k = 0; k < idx.size(); ++k)
                if (matches[idx[k]].HasWarning()) ++fr.warning_count;
            out.files.push_back(fr);
            out.replaced_count += fr.replaced_count;
            out.warning_count  += fr.warning_count;
            ++done;
            continue;
        }

        // 手順7: 一括バックアップ
        std::wstring backup_path;
        if (ctx.create_backup) {
            std::wstring err;
            if (!BackupManager::CreateBackup(e.path, backup_dir, backup_path, err)) {
                e.status = L"失敗";
                e.error  = err;
                fr.status = L"Failed";
                fr.error  = err;
                out.files.push_back(fr);
                ++out.error_count;
                ++done;
                continue;
            }
            fr.backup_file_path = backup_path;
        }

        // 手順8: 一括変換
        std::wstring err;
        FileInfo cur_info;
        FileManager::Inspect(e.path, encoding_mode);
        cur_info = e.info;

        if (!FileManager::WriteTextAtomic(e.path, after, cur_info.encoding,
                                          cur_info.has_bom, cur_info.newline, err)) {
            // 復旧処理（19.1）
            if (!backup_path.empty() && util::PathExists(backup_path)) {
                std::wstring rerr;
                if (FileManager::CopyFileTo(backup_path, e.path, rerr)) {
                    err += L"（バックアップから復旧しました）";
                } else {
                    err += L"（自動復旧にも失敗しました: " + backup_path + L"）";
                }
            }
            e.status = L"失敗";
            e.error  = err;
            fr.status = L"Failed";
            fr.error  = err;
            out.files.push_back(fr);
            ++out.error_count;
            ++done;
            continue;
        }

        // 成功
        e.status = L"成功";
        e.error.clear();
        fr.status = L"Success";
        fr.replaced_count = (int)idx.size();
        for (size_t k = 0; k < idx.size(); ++k)
            if (matches[idx[k]].HasWarning()) ++fr.warning_count;

        out.files.push_back(fr);
        out.replaced_count += fr.replaced_count;
        out.warning_count  += fr.warning_count;

        UndoRecord rec;
        rec.operation_id     = ctx.operation_id;
        rec.target_file_path = e.path;
        rec.backup_file_path = backup_path;
        rec.report_file_path.clear();
        rec.switch_name      = ctx.switch_name;
        rec.executed_at      = ctx.started_at;
        rec.valid            = true;
        out_undo.push_back(rec);

        ++done;
    }

    if (progress) progress(total, total, std::wstring());

    if (dry_run) {
        out.status = (out.warning_count > 0) ? L"Warning" : L"Success";
    } else if (out.error_count > 0) {
        out.status = L"Failed";
    } else if (out.warning_count > 0 || out.skipped_count > 0) {
        out.status = L"Warning";
    } else {
        out.status = L"Success";
    }
    return true;
}

} // namespace ms

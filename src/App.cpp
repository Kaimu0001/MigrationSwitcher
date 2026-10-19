// =============================================================================
// App.cpp
// 設計書 9章「処理フロー」/ 6章「機能詳細設計」/ 13章「状態管理」
// Phase 4: 17章 複数ファイル一括処理 対応
// =============================================================================
#include "App.h"
#include "CodeLogger.h"
#include "HistoryManager.h"
#include "IndentFormatter.h"
#include <fstream>
#include <cstring>
#include "ScopeReportGenerator.h"
#include "GTestGenerator.h"
#include "LogRuntimeGenerator.h"
#include "resource.h"
#include "Utils.h"
#include "BackupManager.h"
#include "BatchProcessor.h"
#include "DiffReportGenerator.h"
#include "EncodingDetector.h"
#include "FileManager.h"
#include "FolderScanner.h"
#include "PreviewGenerator.h"
#include "SearchEngine.h"
#include "SwitchGenerator.h"
#include "SwitchHeaderManager.h"
#include "UndoManager.h"
#include "ValidationManager.h"
#include "CodeScope.h"
#include "ResultExporter.h"

// 文字列から整数を取り出す（数値以外が混ざる場合は既定値）
static int ParseIntOr(const std::wstring& s, int def)
{
    if (s.empty()) return def;
    int v = 0;
    bool any = false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c >= L'0' && c <= L'9') {
            v = v * 10 + (int)(c - L'0');
            any = true;
            if (v > 100000000) return def;
        } else if (c == L' ' || c == L'\t') {
            continue;
        } else {
            return def;
        }
    }
    return any ? v : def;
}


#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>

namespace ms {

// -----------------------------------------------------------------------------
// UI 読み取り
// -----------------------------------------------------------------------------
SwitchStyle App::ReadStyleFromUi() const
{
    for (int i = 0; i < 4; ++i)
        if (::SendMessageW(hRadStyle_[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
            return (SwitchStyle)i;
    return SwitchStyle::IfdefNewFirst;
}

HitBehavior App::ReadHitBehaviorFromUi() const
{
    for (int i = 0; i < 4; ++i)
        if (::SendMessageW(hRadHit_[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
            return (HitBehavior)i;
    return HitBehavior::AutoSelectSingle;
}

PreviewMode App::ReadPreviewModeFromUi() const
{
    int sel = (int)::SendMessageW(hCmbPreviewMode_, CB_GETCURSEL, 0, 0);
    switch (sel) {
    case 1: return PreviewMode::Simple;
    case 2: return PreviewMode::SideBySide;
    case 3: return PreviewMode::Html;
    default: return PreviewMode::TopBottom;
    }
}

TargetKind App::ReadTargetKindFromUi() const
{
    if (hRadTargetKind_[1] &&
        ::SendMessageW(hRadTargetKind_[1], BM_GETCHECK, 0, 0) == BST_CHECKED)
        return TargetKind::Folder;
    return TargetKind::SingleFile;
}

std::vector<std::wstring> App::ReadExtensionsFromUi() const
{
    std::vector<std::wstring> out;
    std::vector<std::wstring> cand = FolderScanner::CandidateExtensions();
    for (size_t i = 0; i < cand.size() && i < 6; ++i)
        if (IsChecked(hChkExt_[i])) out.push_back(cand[i]);
    return out;
}

bool App::IsFolderMode() const
{
    return ReadTargetKindFromUi() == TargetKind::Folder;
}

std::wstring App::ReportDirectory() const
{
    std::wstring d = cfg_.default_report_directory;
    if (d.empty()) d = L"reports";
    if (d.size() > 1 && (d[1] == L':')) return d;
    return util::JoinPath(util::ModuleDirectory(), d);
}

std::wstring App::BackupDirectory() const
{
    std::wstring d = cfg_.default_backup_directory;
    if (d.empty()) return std::wstring();
    if (d.size() > 1 && (d[1] == L':')) return d;
    return util::JoinPath(util::ModuleDirectory(), d);
}

void App::ReadInputs(OperationContext& ctx)
{
    ctx.switch_name      = util::Trim(GetEditText(hSwitchEdit_));
    ctx.old_code         = util::NormalizeNewlines(GetEditText(hOldEdit_));
    ctx.new_code         = util::NormalizeNewlines(GetEditText(hNewEdit_));
    ctx.switch_style     = ReadStyleFromUi();
    ctx.create_backup    = IsChecked(hChkBackup_);
    ctx.create_report    = IsChecked(hChkReport_);
    ctx.auto_indent      = IsChecked(hChkAutoIndent_);
    ctx.started_at       = util::NowDateTime();
    ctx.target_kind      = ReadTargetKindFromUi();
    ctx.extension_filter = ReadExtensionsFromUi();
    ctx.include_subdirs  = IsChecked(hChkSubdirs_);

    ctx.manage_switch_header      = IsChecked(hChkManageSwHeader_);
    ctx.add_switch_header_include = IsChecked(hChkAddInclude_);
    ctx.switch_memo               = util::Trim(GetEditText(hEditSwMemo_));
    ctx.switch_header_path = (ctx.manage_switch_header || ctx.add_switch_header_include)
                             ? CurrentSwitchHeaderPath() : std::wstring();

    if (ctx.target_kind == TargetKind::Folder) {
        ctx.target_folder = util::MakeAbsolute(util::Trim(GetEditText(hFolderEdit_)));
        ctx.target_file_path = ctx.target_folder;
        ctx.target_files = batch_files_;
    } else {
        ctx.target_file_path = util::Trim(GetEditText(hTargetEdit_));
        ctx.target_folder.clear();
    }
}

// -----------------------------------------------------------------------------
// 14章 バリデーション
// -----------------------------------------------------------------------------
bool App::CheckInputs(bool for_execute, std::wstring& out_error,
                      std::vector<std::wstring>& out_warnings)
{
    out_error.clear();
    out_warnings.clear();

    std::wstring sw = util::Trim(GetEditText(hSwitchEdit_));
    std::wstring oc = util::NormalizeNewlines(GetEditText(hOldEdit_));
    std::wstring nc = util::NormalizeNewlines(GetEditText(hNewEdit_));

    // ---- スイッチ定義ヘッダの検証 ----
    if (IsChecked(hChkManageSwHeader_) || IsChecked(hChkAddInclude_)) {
        std::wstring hn = util::Trim(GetEditText(hEditSwHeaderName_));
        if (hn.empty()) hn = SwitchHeaderManager::DefaultHeaderName();
        std::wstring herr;
        if (!SwitchHeaderManager::ValidateHeaderName(hn, herr)) {
            out_error = herr;
            return false;
        }
        std::wstring hp = CurrentSwitchHeaderPath();
        if (util::PathExists(hp)) {
            FileInfo hfi = FileManager::Inspect(hp, L"auto");
            if (hfi.is_directory) {
                out_error = L"スイッチ定義ヘッダの指定先がフォルダです: " + hp;
                return false;
            }
            if (!hfi.writable && !hfi.read_only_attribute) {
                out_error = L"スイッチ定義ヘッダに書き込めません: " + hp;
                return false;
            }
        } else {
            std::wstring hdir = util::GetDirectoryOf(hp);
            if (!hdir.empty() && !util::PathExists(hdir))
                out_warnings.push_back(L"スイッチ定義ヘッダの出力先フォルダを作成します: " + hdir);
        }
    }

    // ---- 一括処理モード（17章）----
    if (IsFolderMode()) {
        std::wstring folder = util::Trim(GetEditText(hFolderEdit_));
        if (folder.empty()) {
            out_error = L"対象フォルダを選択してください。";
            return false;
        }
        if (!util::PathExists(folder)) {
            out_error = L"指定されたフォルダが存在しません: " + folder;
            return false;
        }
        DWORD attr = ::GetFileAttributesW(folder.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            out_error = L"指定されたパスはフォルダではありません。";
            return false;
        }
        if (ReadExtensionsFromUi().empty()) {
            out_error = L"拡張子フィルタを 1 つ以上選択してください。";
            return false;
        }

        ValidationResult vs = ValidationManager::ValidateSwitchName(sw);
        for (size_t i = 0; i < vs.warnings.size(); ++i) out_warnings.push_back(vs.warnings[i]);
        if (!vs.ok) {
            // 検索のみの場合はスイッチ名が無くても実行できる（置換の実行時に必要）
            if (for_execute) { out_error = vs.error; return false; }
            out_warnings.push_back(vs.error + L"（置換の実行時には必要です）");
        }

        ValidationResult vo = ValidationManager::ValidateOldCode(oc);
        for (size_t i = 0; i < vo.warnings.size(); ++i) out_warnings.push_back(vo.warnings[i]);
        if (!vo.ok) { out_error = vo.error; return false; }

        ValidationResult vn = ValidationManager::ValidateNewCode(oc, nc);
        for (size_t i = 0; i < vn.warnings.size(); ++i) out_warnings.push_back(vn.warnings[i]);

        if (for_execute) {
            if (matches_.empty()) {
                out_error = L"検索結果がありません。先に「検索」を実行してください。";
                return false;
            }
            if (GetSelectedMatches().empty()) {
                out_error = L"置換対象が選択されていません。検索結果タブまたは一括処理タブで対象を選択してください。";
                return false;
            }
            if (IsChecked(hChkBackup_)) {
                std::wstring err;
                std::wstring first = GetSelectedFiles().empty() ? std::wstring()
                                                                : GetSelectedFiles()[0];
                if (!first.empty() &&
                    !BackupManager::CanCreateBackup(first, BackupDirectory(), err)) {
                    out_error = L"MSG007: " + err;
                    return false;
                }
            }
        }
        if (IsChecked(hChkDryRunOnly_) && for_execute)
            out_warnings.push_back(L"「ドライランのみ」が有効です。本実行は行われません。");
        return true;
    }

    // ---- 単一ファイルモード ----
    std::wstring path = util::Trim(GetEditText(hTargetEdit_));
    if (path.empty()) { out_error = L"MSG001: 対象ファイルを選択してください。"; return false; }

    FileInfo fi = FileManager::Inspect(util::MakeAbsolute(path), cfg_.encoding_mode);
    ValidationResult vr = ValidationManager::ValidateTargetFile(path, fi);
    for (size_t i = 0; i < vr.warnings.size(); ++i) out_warnings.push_back(vr.warnings[i]);
    if (!vr.ok) { out_error = vr.error; return false; }

    ValidationResult vs = ValidationManager::ValidateSwitchName(sw);
    for (size_t i = 0; i < vs.warnings.size(); ++i) out_warnings.push_back(vs.warnings[i]);
    if (!vs.ok) { out_error = vs.error; return false; }

    ValidationResult vo = ValidationManager::ValidateOldCode(oc);
    for (size_t i = 0; i < vo.warnings.size(); ++i) out_warnings.push_back(vo.warnings[i]);
    if (!vo.ok) { out_error = vo.error; return false; }

    ValidationResult vn = ValidationManager::ValidateNewCode(oc, nc);
    for (size_t i = 0; i < vn.warnings.size(); ++i) out_warnings.push_back(vn.warnings[i]);

    if (for_execute) {
        if (matches_.empty()) {
            out_error = L"検索結果がありません。先に「検索」を実行してください。";
            return false;
        }
        if (GetSelectedMatches().empty()) {
            out_error = L"置換対象が選択されていません。検索結果タブで対象を選択してください。";
            return false;
        }
        if (IsChecked(hChkBackup_)) {
            std::wstring err;
            if (!BackupManager::CanCreateBackup(util::MakeAbsolute(path),
                                                BackupDirectory(), err)) {
                out_error = L"MSG007: " + err;
                return false;
            }
        }
    }

    if (IsChecked(hChkDryRunOnly_) && for_execute)
        out_warnings.push_back(L"「ドライランのみ」が有効です。本実行は行われません。");
    return true;
}

// -----------------------------------------------------------------------------
// 15章 / 16章 警告の付与（単一・一括で共通）
// -----------------------------------------------------------------------------
void App::EvaluateMatchWarnings()
{
    bool warn_pre = IsChecked(hChkWarnPreproc_);
    bool warn_cmt = IsChecked(hChkWarnComment_);
    if (!warn_pre && !warn_cmt) {
        log_.Info(L"警告判定: 無効（基本設定タブの「コメント内の一致を警告する」"
                  L"「既存コンパイルスイッチ近傍を警告する」が未チェックです）");
        return;
    }

    // ファイル単位に本文を取り出す
    int count = 0;
    for (size_t i = 0; i < matches_.size(); ++i) {
        MatchResult& m = matches_[i];

        const std::wstring* text = NULL;
        if (IsFolderMode()) {
            for (size_t k = 0; k < batch_entries_.size(); ++k) {
                if (batch_entries_[k].path == m.file_path) { text = &batch_entries_[k].text; break; }
            }
        } else {
            if (m.file_path == target_path_ || target_path_.empty()) text = &file_text_;
        }
        if (!text) continue;

        if (warn_pre) {
            std::wstring detail;
            if (SearchEngine::DetectNearbyPreprocessor(*text, m.start_line, m.end_line,
                                                       cfg_.preprocessor_scan_lines, detail)) {
                m.warning_flags.push_back(
                    detail + L"二重にコンパイルスイッチで囲われる可能性があります。");
                if (m.status == MatchStatus::Target) m.status = MatchStatus::Warning;
                ++count;
            }
        }
        if (warn_cmt) {
            if (SearchEngine::IsInsideComment(*text, m.start_offset)) {
                m.warning_flags.push_back(
                    L"コメント内（// または /* */）で一致しています。"
                    L"意図した置換対象か確認してください。");
                if (m.status == MatchStatus::Target) m.status = MatchStatus::Warning;
                ++count;
            }
        }
    }
    if (count > 0) {
        log_.Warn(L"警告付きの一致箇所: " + std::to_wstring(count) + L" 件");
        for (size_t i = 0; i < matches_.size(); ++i) {
            const MatchResult& m = matches_[i];
            if (!m.HasWarning()) continue;
            for (size_t k = 0; k < m.warning_flags.size(); ++k)
                log_.Warn(L"  " + util::GetFileNameOf(m.file_path) + L" " +
                          std::to_wstring(m.start_line) + L" 行目: " + m.warning_flags[k]);
        }
    } else if (warn_pre || warn_cmt) {
        log_.Info(L"警告判定: 該当なし（コメント内の一致 / 既存スイッチ近傍なし）");
    }
}

// -----------------------------------------------------------------------------
// 6.6 複数ヒット処理 / 5.3.1 ヒット時の動作
// -----------------------------------------------------------------------------
void App::ApplyHitBehavior()
{
    for (size_t i = 0; i < matches_.size(); ++i) matches_[i].selected = false;

    switch (ReadHitBehaviorFromUi()) {
    case HitBehavior::AutoSelectSingle:
        if (matches_.size() == 1) matches_[0].selected = true;
        break;
    case HitBehavior::FirstOnly:
        if (!matches_.empty()) matches_[0].selected = true;
        break;
    case HitBehavior::AllMatches:
        for (size_t i = 0; i < matches_.size(); ++i) matches_[i].selected = true;
        break;
    case HitBehavior::Manual:
    default:
        break;
    }
}

std::vector<std::wstring> App::GetSelectedFiles() const
{
    std::vector<std::wstring> out;
    for (size_t i = 0; i < batch_entries_.size(); ++i)
        if (batch_entries_[i].selected) out.push_back(batch_entries_[i].path);
    return out;
}

std::vector<MatchResult> App::GetSelectedMatches() const
{
    std::vector<MatchResult> out;
    for (size_t i = 0; i < matches_.size(); ++i) {
        if (!matches_[i].selected) continue;
        if (IsFolderMode()) {
            // 一括処理: ファイル単位の選択も反映する
            bool file_on = false;
            for (size_t k = 0; k < batch_entries_.size(); ++k) {
                if (batch_entries_[k].path == matches_[i].file_path) {
                    file_on = batch_entries_[k].selected;
                    break;
                }
            }
            if (!file_on) continue;
        }
        out.push_back(matches_[i]);
    }
    return out;
}

// -----------------------------------------------------------------------------
// 一覧表示
// -----------------------------------------------------------------------------
void App::RefreshListView()
{
    if (!hListView_) return;
    suppress_events_ = true;

    ::SendMessageW(hListView_, LVM_DELETEALLITEMS, 0, 0);
    ::SendMessageW(hListView_, WM_SETREDRAW, FALSE, 0);

    for (size_t i = 0; i < matches_.size(); ++i) {
        const MatchResult& m = matches_[i];

        std::wstring type = L"完全一致";
        if (m.match_type == MatchType::Normalized) type = L"正規化一致";
        else if (m.match_type == MatchType::Similar) type = L"類似一致";

        std::wstring st = L"対象";
        if (m.status == MatchStatus::Excluded) st = L"除外";
        else if (m.status == MatchStatus::Warning) st = L"警告";
        else if (m.status == MatchStatus::Error) st = L"エラー";

        std::wstring preview = m.matched_text;
        for (size_t k = 0; k < preview.size(); ++k) {
            if (preview[k] == L'\n') preview[k] = L' ';
            if (preview[k] == L'\t') preview[k] = L' ';
        }
        if (preview.size() > 120) preview = preview.substr(0, 120) + L"...";

        LVITEMW it;
        ::ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.iSubItem = 0;
        std::wstring no = std::to_wstring(m.match_id);
        it.pszText = (LPWSTR)no.c_str();
        int row = (int)::SendMessageW(hListView_, LVM_INSERTITEMW, 0, (LPARAM)&it);

        auto setSub = [&](int sub, const std::wstring& text) {
            LVITEMW si;
            ::ZeroMemory(&si, sizeof(si));
            si.iItem = row;
            si.iSubItem = sub;
            si.mask = LVIF_TEXT;
            si.pszText = (LPWSTR)text.c_str();
            ::SendMessageW(hListView_, LVM_SETITEMW, 0, (LPARAM)&si);
        };

        setSub(1, m.selected ? L"○" : L"");
        setSub(2, util::GetFileNameOf(m.file_path));
        setSub(3, std::to_wstring(m.start_line));
        setSub(4, std::to_wstring(m.start_column));
        setSub(5, type);
        setSub(6, std::to_wstring(m.match_rate) + L"%");
        setSub(7, st);
        setSub(8, preview);

        ListView_SetCheckState(hListView_, row, m.selected ? TRUE : FALSE);
    }

    ::SendMessageW(hListView_, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(hListView_, nullptr, TRUE);

    wchar_t buf[256];
    swprintf(buf, 256, L"ヒット件数: %d 件 / 選択中: %d 件",
             (int)matches_.size(), (int)GetSelectedMatches().size());
    SetEditText(hLblResultInfo_, buf);

    suppress_events_ = false;
}

void App::RefreshFileList()
{
    if (!hListFiles_) return;
    suppress_events_ = true;

    ::SendMessageW(hListFiles_, LVM_DELETEALLITEMS, 0, 0);
    ::SendMessageW(hListFiles_, WM_SETREDRAW, FALSE, 0);

    for (size_t i = 0; i < batch_entries_.size(); ++i) {
        BatchFileEntry& e = batch_entries_[i];

        int sel_count = 0;
        for (size_t k = 0; k < matches_.size(); ++k)
            if (matches_[k].file_path == e.path && matches_[k].selected) ++sel_count;
        e.selected_count = sel_count;

        LVITEMW it;
        ::ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.iSubItem = 0;
        std::wstring no = std::to_wstring((int)i + 1);
        it.pszText = (LPWSTR)no.c_str();
        int row = (int)::SendMessageW(hListFiles_, LVM_INSERTITEMW, 0, (LPARAM)&it);

        auto setSub = [&](int sub, const std::wstring& text) {
            LVITEMW si;
            ::ZeroMemory(&si, sizeof(si));
            si.iItem = row;
            si.iSubItem = sub;
            si.mask = LVIF_TEXT;
            si.pszText = (LPWSTR)text.c_str();
            ::SendMessageW(hListFiles_, LVM_SETITEMW, 0, (LPARAM)&si);
        };

        std::wstring st = e.status.empty() ? std::wstring(L"待機") : e.status;
        if (!e.error.empty()) st += L"（" + e.error + L"）";

        setSub(1, util::GetFileNameOf(e.path));
        setSub(2, util::GetDirectoryOf(e.path));
        setSub(3, std::to_wstring(e.hit_count));
        setSub(4, std::to_wstring(e.selected_count));
        setSub(5, st);

        ListView_SetCheckState(hListFiles_, row, e.selected ? TRUE : FALSE);
    }

    ::SendMessageW(hListFiles_, WM_SETREDRAW, TRUE, 0);
    ::InvalidateRect(hListFiles_, nullptr, TRUE);

    suppress_events_ = false;
    RefreshScanInfo();
}

void App::RefreshScanInfo()
{
    if (!hLblScanInfo_) return;
    int hit_files = 0, total_hits = 0, sel_files = 0;
    for (size_t i = 0; i < batch_entries_.size(); ++i) {
        if (batch_entries_[i].hit_count > 0) ++hit_files;
        total_hits += batch_entries_[i].hit_count;
        if (batch_entries_[i].selected) ++sel_files;
    }
    wchar_t buf[320];
    if (batch_entries_.empty()) {
        swprintf(buf, 320, L"対象: %d ファイル（未検索）",
                 (int)batch_files_.size());
    } else {
        swprintf(buf, 320, L"走査 %d ファイル / ヒット %d ファイル %d 件 / 対象 %d ファイル",
                 (int)batch_files_.size(), hit_files, total_hits, sel_files);
    }
    SetEditText(hLblScanInfo_, buf);
}

void App::SyncSelectionFromList()
{
    int n = ListView_GetItemCount(hListView_);
    if (n != (int)matches_.size()) return;
    for (int i = 0; i < n; ++i)
        matches_[(size_t)i].selected =
            ListView_GetCheckState(hListView_, i) ? true : false;
    UpdateSelectionColumn();
    RefreshFileList();
}

void App::SyncFileSelectionFromList()
{
    int n = ListView_GetItemCount(hListFiles_);
    if (n != (int)batch_entries_.size()) return;
    for (int i = 0; i < n; ++i)
        batch_entries_[(size_t)i].selected =
            ListView_GetCheckState(hListFiles_, i) ? true : false;
    RefreshFileList();
    UpdateSelectionColumn();
}

void App::UpdateSelectionColumn()
{
    if (!hListView_) return;
    suppress_events_ = true;
    int n = ListView_GetItemCount(hListView_);
    for (int i = 0; i < n && i < (int)matches_.size(); ++i) {
        std::wstring mark = matches_[(size_t)i].selected ? L"○" : L"";
        LVITEMW si;
        ::ZeroMemory(&si, sizeof(si));
        si.iItem = i;
        si.iSubItem = 1;
        si.mask = LVIF_TEXT;
        si.pszText = (LPWSTR)mark.c_str();
        ::SendMessageW(hListView_, LVM_SETITEMW, 0, (LPARAM)&si);
    }
    wchar_t buf[256];
    swprintf(buf, 256, L"ヒット件数: %d 件 / 選択中: %d 件",
             (int)matches_.size(), (int)GetSelectedMatches().size());
    SetEditText(hLblResultInfo_, buf);
    suppress_events_ = false;
}

void App::DoSelectAll()
{
    for (size_t i = 0; i < matches_.size(); ++i) matches_[i].selected = true;
    RefreshListView();
    RefreshFileList();
    log_.Info(L"すべての一致を置換対象に設定しました。");
}

void App::DoClearSelection()
{
    for (size_t i = 0; i < matches_.size(); ++i) matches_[i].selected = false;
    RefreshListView();
    RefreshFileList();
    log_.Info(L"置換対象の選択を解除しました。");
}

void App::DoFileSelectAll()
{
    for (size_t i = 0; i < batch_entries_.size(); ++i) batch_entries_[i].selected = true;
    RefreshFileList();
    UpdateSelectionColumn();
    log_.Info(L"すべてのファイルを対象に設定しました。");
}

void App::DoFileSelectNone()
{
    for (size_t i = 0; i < batch_entries_.size(); ++i) batch_entries_[i].selected = false;
    RefreshFileList();
    UpdateSelectionColumn();
    log_.Info(L"すべてのファイルを対象外にしました。");
}

void App::DoClearLog()
{
    ::SetWindowTextW(hLogEdit_, L"");
}

void App::OnTargetKindChanged()
{
    bool folder = IsFolderMode();
    ::EnableWindow(hTargetEdit_, !folder);
    ::EnableWindow(hBrowseBtn_, !folder);
    ::EnableWindow(hFolderEdit_, folder);
    ::EnableWindow(hFolderBrowseBtn_, folder);
    // 走査は新旧コード・スイッチ名が未入力でも実行できる（フォルダ走査のみ）
    ::EnableWindow(hBtnScan_, TRUE);
    ::EnableWindow(hBtnFileSelAll_, folder);
    ::EnableWindow(hBtnFileSelNone_, folder);
    ::EnableWindow(hListFiles_, folder);
    for (int i = 0; i < 6; ++i) ::EnableWindow(hChkExt_[i], folder);
    ::EnableWindow(hChkSubdirs_, folder);
    SetStatus(folder ? L"フォルダモード: 対象フォルダと拡張子フィルタを指定して「走査」→「検索」"
                     : L"Ready");
}

// -----------------------------------------------------------------------------
// 状態管理（13章）
// -----------------------------------------------------------------------------
void App::SetState(AppState s)
{
    state_ = s;
    UpdateButtons();

    std::wstring st;
    switch (s) {
    case AppState::INIT:           st = L"初期化中..."; break;
    case AppState::READY:          st = L"Ready"; break;
    case AppState::FILE_SELECTED:  st = L"ファイル選択済み"; break;
    case AppState::SEARCHED:       st = L"検索完了（" + std::to_wstring((int)matches_.size()) + L" 件）"; break;
    case AppState::PREVIEWED:      st = L"プレビュー生成済み"; break;
    case AppState::EXECUTING:      st = L"実行中..."; break;
    case AppState::COMPLETED:      st = L"完了"; break;
    case AppState::Error:          st = L"エラー"; break;
    case AppState::UNDO_AVAILABLE: st = L"完了（元に戻す が可能です）"; break;
    default:                       st = L"Ready"; break;
    }
    SetStatus(st);
}

void App::UpdateButtons()
{
    bool exec_running = (state_ == AppState::EXECUTING);

    bool can_search = !exec_running &&
        (state_ == AppState::READY || state_ == AppState::FILE_SELECTED ||
         state_ == AppState::SEARCHED || state_ == AppState::PREVIEWED ||
         state_ == AppState::COMPLETED || state_ == AppState::Error ||
         state_ == AppState::UNDO_AVAILABLE);

    bool can_use = !exec_running &&
        (state_ == AppState::SEARCHED || state_ == AppState::PREVIEWED ||
         state_ == AppState::COMPLETED || state_ == AppState::UNDO_AVAILABLE);

    bool can_undo = !exec_running && undo_.CanUndo();

    ::EnableWindow(hBtnSearch_,  can_search);
    ::EnableWindow(hBtnPreview_, can_use);
    ::EnableWindow(hBtnDryRun_,  can_use);
    ::EnableWindow(hBtnExecute_, can_use);
    ::EnableWindow(hBtnUndo_,    can_undo);
}

// -----------------------------------------------------------------------------
// 6.1 対象ファイル選択
// -----------------------------------------------------------------------------
void App::DoBrowse()
{
    std::wstring initial = util::Trim(GetEditText(hTargetEdit_));
    if (initial.empty()) {
        initial = cfg_.last_target_directory;
        if (initial.empty()) initial = util::ModuleDirectory();
    }

    wchar_t buf[MAX_PATH * 4];
    ::ZeroMemory(buf, sizeof(buf));
    lstrcpynW(buf, initial.c_str(), MAX_PATH * 4);

    OPENFILENAMEW ofn;
    ::ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hWnd_;
    ofn.lpstrFilter =
        L"C/C++ ソース (*.c;*.cpp;*.h;*.hpp;*.cc;*.cxx)\0*.c;*.cpp;*.h;*.hpp;*.cc;*.cxx\0"
        L"テキスト ファイル (*.txt)\0*.txt\0"
        L"すべてのファイル (*.*)\0*.*\0\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH * 4;
    ofn.lpstrTitle  = L"対象ファイルを選択";
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!::GetOpenFileNameW(&ofn)) {
        if (util::Trim(GetEditText(hTargetEdit_)).empty()) return;
    } else {
        SetEditText(hTargetEdit_, buf);
    }

    target_path_ = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
    file_info_ = FileManager::Inspect(target_path_, cfg_.encoding_mode);
    cfg_.last_target_directory = util::GetDirectoryOf(target_path_);

    log_.Info(L"Target file selected: " + target_path_);
    log_.Info(L"  Encoding: " + std::wstring(EncodingDetector::EncodingName(file_info_.encoding)) +
              L" / Newline: " +
              (file_info_.newline == NewlineKind::CRLF ? L"CRLF" :
               (file_info_.newline == NewlineKind::CR ? L"CR" : L"LF")) +
              L" / Size: " + std::to_wstring((long long)file_info_.size) + L" bytes");

    ValidationResult vr = ValidationManager::ValidateTargetFile(target_path_, file_info_);
    if (!vr.ok) {
        ShowError(vr.error);
        log_.Error(vr.error);
        SetState(AppState::Error);
        return;
    }
    for (size_t i = 0; i < vr.warnings.size(); ++i) log_.Warn(vr.warnings[i]);

    if (!FileManager::ReadText(target_path_, cfg_.encoding_mode, file_text_, file_info_)) {
        ShowError(L"ファイルを読み取れません。");
        log_.Error(L"Failed to read file: " + target_path_);
        SetState(AppState::Error);
        return;
    }

    matches_.clear();
    batch_entries_.clear();
    batch_files_.clear();
    RefreshListView();
    RefreshFileList();
    if (!vr.warnings.empty()) ShowWarn(ValidationManager::BuildWarningSummary(vr.warnings));
    SetState(AppState::FILE_SELECTED);
}

// -----------------------------------------------------------------------------
// 17.2.1 対象フォルダ選択
// -----------------------------------------------------------------------------
void App::DoFolderBrowse()
{
    std::wstring initial = util::Trim(GetEditText(hFolderEdit_));
    if (initial.empty()) initial = cfg_.last_target_folder;
    if (initial.empty()) initial = util::ModuleDirectory();

    BROWSEINFOW bi;
    ::ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hWnd_;
    bi.lpszTitle = L"対象フォルダを選択してください";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

    LPITEMIDLIST pidl = ::SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t path[MAX_PATH * 2];
    path[0] = 0;
    if (::SHGetPathFromIDListW(pidl, path)) {
        SetEditText(hFolderEdit_, path);
        cfg_.last_target_folder = path;
        log_.Info(L"Target folder selected: " + std::wstring(path));
    }
    ::CoTaskMemFree(pidl);
}

// -----------------------------------------------------------------------------
// 17.2.2 手順1-2 走査
// -----------------------------------------------------------------------------
void App::DoScan()
{
    std::wstring folder = util::Trim(GetEditText(hFolderEdit_));
    if (folder.empty()) { ShowError(L"対象フォルダを選択してください。"); return; }

    folder = util::MakeAbsolute(folder);
    std::vector<std::wstring> exts = ReadExtensionsFromUi();
    if (exts.empty()) {
        ShowError(L"拡張子フィルタを 1 つ以上選択してください。");
        return;
    }
    bool sub = IsChecked(hChkSubdirs_);

    log_.Info(L"Scan started. Folder: " + folder);
    SetProgress(0);

    ScanResult sr;
    FolderScanner::Scan(folder, exts, sub, sr);

    batch_files_ = sr.files;
    batch_entries_.clear();
    matches_.clear();
    batch_folder_ = folder;
    cfg_.last_target_folder = folder;

    RefreshListView();
    RefreshFileList();

    log_.Info(L"Scan completed. Files: " + std::to_wstring((int)sr.files.size()) +
              L" / Directories: " + std::to_wstring(sr.scanned_directories));

    if (sr.files.empty()) {
        SetStatus(L"走査完了: 対象ファイルが 0 件です。フォルダと拡張子フィルタを確認してください。");
        ShowWarn(L"条件に一致するファイルが見つかりませんでした。\n\n"
                 L"対象フォルダと拡張子フィルタを確認してください。");
    } else {
        SetStatus(L"走査完了: " + std::to_wstring((int)sr.files.size()) +
                  L" ファイル（新旧コードは未入力でも走査できます）。"
                  L"次に旧コード・新コードを入力して「検索」を押してください。");
    }
    for (size_t i = 0; i < sr.errors.size() && i < 20; ++i) log_.Warn(sr.errors[i]);
    for (size_t i = 0; i < sr.skipped.size() && i < 20; ++i) log_.Warn(L"除外: " + sr.skipped[i]);

    if (!sr.errors.empty()) ShowWarn(ValidationManager::BuildWarningSummary(sr.errors));

    if (sr.files.empty()) {
        ShowInfo(L"条件に一致するファイルが見つかりませんでした。\n"
                 L"対象フォルダと拡張子フィルタを確認してください。");
        SetState(AppState::FILE_SELECTED);
        return;
    }

    if (sr.truncated) {
        ShowWarn(L"対象ファイル数が上限（" + std::to_wstring(FolderScanner::kMaxFiles) +
                 L" 件）に達したため、以降のファイルは走査していません。");
    }
    SetProgress(100);
    SetState(AppState::FILE_SELECTED);
    ShowInfo(L"走査が完了しました。\n\n対象ファイル: " + std::to_wstring((int)sr.files.size()) +
             L" 件\n\n「検索」を実行してください。");
}

// -----------------------------------------------------------------------------
// 9.2 検索処理フロー
// -----------------------------------------------------------------------------
void App::DoSearch()
{
    if (IsFolderMode()) { DoBatchSearch(); return; }

    std::wstring err;
    std::vector<std::wstring> warns;
    if (!CheckInputs(false, err, warns)) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }
    for (size_t i = 0; i < warns.size(); ++i) log_.Warn(warns[i]);

    target_path_ = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
    log_.Info(L"Search started.");

    if (!FileManager::ReadText(target_path_, cfg_.encoding_mode, file_text_, file_info_)) {
        ShowError(L"ファイルを読み取れません。");
        log_.Error(L"Failed to read file: " + target_path_);
        SetState(AppState::Error);
        return;
    }

    std::wstring pattern = util::NormalizeNewlines(GetEditText(hOldEdit_));
    if (!pattern.empty() && pattern[pattern.size() - 1] == L'\n') {
        pattern.erase(pattern.size() - 1);
        log_.Info(L"差し替え対象コード末尾の改行を除去して検索します。");
    }
    if (util::IsBlank(pattern)) {
        ShowError(L"MSG003: 差し替え対象コードを入力してください。");
        SetState(AppState::Error);
        return;
    }

    std::wstring search_err;
    matches_ = RunConfiguredSearch(file_text_, pattern, target_path_, search_err);
    if (!search_err.empty()) {
        ShowError(search_err);
        log_.Error(search_err);
        SetState(AppState::Error);
        return;
    }
    SearchEngine::Renumber(matches_);

    log_.Info(L"Match count: " + std::to_wstring((int)matches_.size()) + L".");
    for (size_t i = 0; i < matches_.size(); ++i) {
        log_.Info(L"Match found at line " + std::to_wstring(matches_[i].start_line) +
                  L", column " + std::to_wstring(matches_[i].start_column) + L".");
    }

    EvaluateMatchWarnings();

    if (matches_.empty()) {
        RefreshListView();
        SetState(AppState::SEARCHED);
        ShowInfo(L"MSG005: 対象コードが見つかりませんでした。");
        log_.Warn(L"対象コードが見つかりませんでした。");
        return;
    }

    ApplyHitBehavior();
    RefreshListView();

    if (matches_.size() > 1 && ReadHitBehaviorFromUi() == HitBehavior::AutoSelectSingle) {
        ShowWarn(L"MSG006: 同一コードが " + std::to_wstring((int)matches_.size()) +
                 L" 件見つかりました。\n置換対象を選択してください。");
    }

    int warn_count = 0;
    for (size_t i = 0; i < matches_.size(); ++i)
        if (matches_[i].HasWarning()) ++warn_count;
    if (warn_count > 0)
        ShowWarn(L"警告付きの一致箇所が " + std::to_wstring(warn_count) +
                 L" 件あります。検索結果タブの「状態」列を確認してください。");

    SetState(AppState::SEARCHED);
    SetProgress(100);
}

// -----------------------------------------------------------------------------
// 17.2.2 手順3-4 一括検索
// -----------------------------------------------------------------------------
void App::DoBatchSearch()
{
    std::wstring err;
    std::vector<std::wstring> warns;
    if (!CheckInputs(false, err, warns)) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }
    for (size_t i = 0; i < warns.size(); ++i) log_.Warn(warns[i]);

    std::wstring folder = util::MakeAbsolute(util::Trim(GetEditText(hFolderEdit_)));
    std::vector<std::wstring> exts = ReadExtensionsFromUi();
    bool sub = IsChecked(hChkSubdirs_);

    std::wstring pattern = util::NormalizeNewlines(GetEditText(hOldEdit_));
    if (!pattern.empty() && pattern[pattern.size() - 1] == L'\n') {
        pattern.erase(pattern.size() - 1);
        log_.Info(L"差し替え対象コード末尾の改行を除去して検索します。");
    }
    if (util::IsBlank(pattern)) {
        ShowError(L"MSG003: 差し替え対象コードを入力してください。");
        SetState(AppState::Error);
        return;
    }

    log_.Info(L"Batch search started. Folder: " + folder);
    SetProgress(0);

    BatchSearchResult bs;
    BatchProgressFn prog = [this](int done, int total, const std::wstring& cur) {
        if (total > 0) SetProgress((int)((done * 100) / total));
        if (!cur.empty())
            SetStatus(L"検索中 (" + std::to_wstring(done) + L"/" + std::to_wstring(total) +
                      L"): " + util::GetFileNameOf(cur));
    };

    if (!BatchProcessor::SearchFolder(folder, exts, sub, pattern, cfg_.encoding_mode,
                                      prog, bs, err)) {
        batch_files_ = bs.all_files;
        batch_entries_.clear();
        matches_.clear();
        RefreshListView();
        RefreshFileList();
        ShowInfo(L"MSG005: " + err);
        log_.Warn(err);
        SetState(AppState::SEARCHED);
        return;
    }

    batch_folder_  = folder;
    batch_files_   = bs.all_files;
    batch_entries_ = bs.entries;
    matches_       = bs.matches;
    ApplySearchOptionsToBatch(pattern);
    SearchEngine::Renumber(matches_);

    for (size_t i = 0; i < bs.errors.size() && i < 20; ++i) log_.Warn(bs.errors[i]);

    log_.Info(L"Scanned files: " + std::to_wstring(bs.scanned_count));
    log_.Info(L"Hit files: " + std::to_wstring(bs.hit_file_count) +
              L" / Match count: " + std::to_wstring(bs.total_matches));
    for (size_t i = 0; i < bs.entries.size(); ++i)
        log_.Info(L"  " + bs.entries[i].path + L" : " +
                  std::to_wstring(bs.entries[i].hit_count) + L" 件");

    EvaluateMatchWarnings();
    ApplyHitBehavior();

    if (matches_.empty()) {
        RefreshListView();
        RefreshFileList();
        SetState(AppState::SEARCHED);
        ShowInfo(L"MSG005: 対象コードが見つかりませんでした。\n\n走査ファイル: " +
                 std::to_wstring(bs.scanned_count) + L" 件");
        log_.Warn(L"対象コードが見つかりませんでした。");
        return;
    }

    RefreshListView();
    RefreshFileList();

    if (matches_.size() > 1 && ReadHitBehaviorFromUi() == HitBehavior::AutoSelectSingle) {
        ShowWarn(L"MSG006: 同一コードが " + std::to_wstring((int)matches_.size()) +
                 L" 件（" + std::to_wstring(bs.hit_file_count) +
                 L" ファイル）見つかりました。\n置換対象を選択してください。");
    }

    int warn_count = 0;
    for (size_t i = 0; i < matches_.size(); ++i)
        if (matches_[i].HasWarning()) ++warn_count;
    if (warn_count > 0)
        ShowWarn(L"警告付きの一致箇所が " + std::to_wstring(warn_count) +
                 L" 件あります。検索結果タブの「状態」列を確認してください。");

    SetProgress(100);
    SetState(AppState::SEARCHED);
}

// -----------------------------------------------------------------------------
// 6.7 / 6.8 置換ブロックの生成
// -----------------------------------------------------------------------------
std::wstring App::MakeBlockFor(const MatchResult& m, const OperationContext& ctx,
                               const std::wstring& text) const
{
    // 一致行のインデント（行頭から一致箇所までが空白のときのみ有効）
    std::wstring hint = SearchEngine::IndentHintAt(text, m);

    // 追加コードモード: 一致した「直前コード」の直後へ、整形した追加コードを置く
    if (add_code_mode_) {
        std::wstring body = ctx.auto_indent
            ? IndentFormatter::FormatBlock(add_code_, hint)
            : add_code_;
        std::wstring base = m.matched_text.empty() ? ctx.old_code : m.matched_text;
        return base + L"\n" + body;
    }

    std::wstring oc = ctx.old_code;
    std::wstring nc = ctx.new_code;
    if (ctx.auto_indent) {
        // 相対的な深さ（IF 文の内側など）を保ったまま、一致行のインデントへ合わせる
        oc = IndentFormatter::FormatBlock(oc, hint);
        nc = IndentFormatter::FormatBlock(nc, hint);
    }
    std::vector<std::wstring> gw;
    return SwitchGenerator::GenerateEx(
        ctx.switch_style, ctx.switch_name, oc, nc,
        false, GetEditText(hEditTemplate_), hint, gw);
}

std::vector<std::wstring> App::MakeBlocksFor(const std::vector<MatchResult>& ms,
                                             const OperationContext& ctx,
                                             const std::wstring& text) const
{
    std::vector<std::wstring> out;
    out.reserve(ms.size());
    for (size_t i = 0; i < ms.size(); ++i)
        out.push_back(MakeBlockFor(ms[i], ctx, text));
    return out;
}

// -----------------------------------------------------------------------------
// 9.3 プレビュー処理フロー（単一ファイル）
// -----------------------------------------------------------------------------
bool App::BuildPreviewInternal(std::wstring& out_error)
{
    out_error.clear();

    if (matches_.empty()) {
        out_error = L"検索結果がありません。先に「検索」を実行してください。";
        return false;
    }
    std::vector<MatchResult> sel = GetSelectedMatches();
    if (sel.empty()) {
        out_error = L"置換対象が選択されていません。検索結果タブで対象を選択してください。";
        return false;
    }

    OperationContext ctx;
    ReadInputs(ctx);
    ctx.selected_matches = sel;
    ctx.encoding = file_info_.encoding;
    ctx.newline = file_info_.newline;

    std::vector<std::wstring> blocks = MakeBlocksFor(sel, ctx, file_text_);
    std::wstring after = PreviewGenerator::BuildAfterTextEach(file_text_, sel, blocks);
    ApplyLogInsertion(after, sel);

    std::wstring before_ex, after_ex;
    if (!PreviewGenerator::BuildExcerpts(file_text_, after, sel, 4, before_ex, after_ex)) {
        out_error = L"プレビューの生成に失敗しました。";
        return false;
    }

    PreviewMode pm = ReadPreviewModeFromUi();
    std::wstring shown;
    if (pm == PreviewMode::Simple)          shown = PreviewGenerator::BuildSimple(after_ex);
    else if (pm == PreviewMode::SideBySide) shown = PreviewGenerator::BuildSideBySide(before_ex, after_ex);
    else                                    shown = PreviewGenerator::BuildTopBottom(before_ex, after_ex);

    std::wstring head;
    head += L"===== 生成されるコンパイルスイッチ（方式: ";
    head += SwitchGenerator::StyleName(ctx.switch_style);
    head += L"）=====\r\n\r\n";
    head += util::DenormalizeNewlines(blocks[0], L"\r\n");
    head += L"\r\n\r\n";

    ::SetWindowTextW(hPreviewEdit_, (head + shown).c_str());

    ::SendMessageW(hTab_, TCM_SETCURSEL, PG_PREVIEW, 0);
    OnTabChange();
    return true;
}

// -----------------------------------------------------------------------------
// 17.2.2 手順6 一括プレビュー
// -----------------------------------------------------------------------------
bool App::BuildBatchPreview(std::wstring& out_error)
{
    out_error.clear();

    if (matches_.empty()) {
        out_error = L"検索結果がありません。先に「検索」を実行してください。";
        return false;
    }
    std::vector<MatchResult> sel = GetSelectedMatches();
    if (sel.empty()) {
        out_error = L"置換対象が選択されていません。検索結果タブまたは一括処理タブで対象を選択してください。";
        return false;
    }

    OperationContext ctx;
    ReadInputs(ctx);

    // ファイル単位にまとめる（一括処理タブの並び順）
    std::vector<size_t> order;
    for (size_t ei = 0; ei < batch_entries_.size(); ++ei) {
        if (!batch_entries_[ei].selected) continue;
        for (size_t mi = 0; mi < sel.size(); ++mi)
            if (sel[mi].file_path == batch_entries_[ei].path) { order.push_back(ei); break; }
    }

    std::wstring out;
    wchar_t buf[512];
    swprintf(buf, 512,
             L"===== 一括プレビュー =====\r\n"
             L"対象フォルダ : %s\r\n"
             L"スイッチ名   : %s\r\n"
             L"置換方式     : %s\r\n"
             L"対象ファイル : %d 件 / 置換件数 : %d 件\r\n\r\n",
             batch_folder_.c_str(), ctx.switch_name.c_str(),
             SwitchGenerator::StyleName(ctx.switch_style),
             (int)order.size(), (int)sel.size());
    out += buf;

    const size_t kMaxFiles = 50;
    size_t shown = 0;
    for (size_t oi = 0; oi < order.size(); ++oi) {
        if (shown >= kMaxFiles) {
            out += L"...（残り " + std::to_wstring((int)order.size() - (int)shown) +
                   L" ファイルは省略。実行後に統合レポートで確認できます）\r\n";
            break;
        }
        BatchFileEntry& e = batch_entries_[order[oi]];

        std::vector<MatchResult> fm;
        std::vector<std::wstring> fb;
        for (size_t mi = 0; mi < sel.size(); ++mi) {
            if (sel[mi].file_path != e.path) continue;
            fm.push_back(sel[mi]);
            fb.push_back(MakeBlockFor(sel[mi], ctx, e.text));
        }
        if (fm.empty()) continue;

        std::wstring after = PreviewGenerator::BuildAfterTextEach(e.text, fm, fb);

        std::wstring before_ex, after_ex;
        PreviewGenerator::BuildExcerpts(e.text, after, fm, 3, before_ex, after_ex);

        out += L"--------------------------------------------------------------------\r\n";
        out += L"ファイル: " + e.path + L"\r\n";
        swprintf(buf, 512, L"  ヒット %d 件 / 置換 %d 件 / 文字コード %s / 改行 %s\r\n",
                 e.hit_count, (int)fm.size(),
                 EncodingDetector::EncodingName(e.info.encoding),
                 e.info.newline == NewlineKind::CRLF ? L"CRLF" :
                 (e.info.newline == NewlineKind::CR ? L"CR" : L"LF"));
        out += buf;
        out += L"--------------------------------------------------------------------\r\n";

        PreviewMode pm = ReadPreviewModeFromUi();
        if (pm == PreviewMode::Simple)          out += PreviewGenerator::BuildSimple(after_ex);
        else if (pm == PreviewMode::SideBySide) out += PreviewGenerator::BuildSideBySide(before_ex, after_ex);
        else                                    out += PreviewGenerator::BuildTopBottom(before_ex, after_ex);
        out += L"\r\n";
        ++shown;
    }

    ::SetWindowTextW(hPreviewEdit_, out.c_str());
    ::SendMessageW(hTab_, TCM_SETCURSEL, PG_PREVIEW, 0);
    OnTabChange();
    return true;
}

void App::DoPreview()
{
    std::wstring err;
    log_.Info(L"Preview started.");
    bool ok = IsFolderMode() ? BuildBatchPreview(err) : BuildPreviewInternal(err);
    if (!ok) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }
    log_.Info(L"Preview generated.");
    SetState(AppState::PREVIEWED);
}

void App::DoPreviewModeChanged()
{
    if (matches_.empty()) return;
    std::wstring err;
    if (IsFolderMode()) { if (!BuildBatchPreview(err)) return; }
    else                { if (!BuildPreviewInternal(err)) return; }
    log_.Info(L"プレビュー表示モードを変更しました。");
}

// -----------------------------------------------------------------------------
// 9.5 ドライラン / 9.4 本実行
// -----------------------------------------------------------------------------
void App::DoDryRun()
{
    if (IsFolderMode()) RunBatchOperation(ExecutionMode::DryRun);
    else                RunOperation(ExecutionMode::DryRun);
}

void App::DoExecute()
{
    if (IsChecked(hChkDryRunOnly_)) {
        ShowInfo(L"「ドライランのみ」が有効です。ドライランを実行します。");
        if (IsFolderMode()) RunBatchOperation(ExecutionMode::DryRun);
        else                RunOperation(ExecutionMode::DryRun);
        return;
    }
    // 変更前スナップショット（履歴 / 比較用）
    std::vector<HistoryFileEntry> captured;
    CaptureBeforeSnapshot(captured);

    if (IsFolderMode()) RunBatchOperation(ExecutionMode::Real);
    else                RunOperation(ExecutionMode::Real);

    RecordHistoryAfterRun(captured, L"Real");
}

// -----------------------------------------------------------------------------
// 9.4 本実行処理フロー（単一ファイル）
// -----------------------------------------------------------------------------
void App::RunOperation(ExecutionMode mode)
{
    std::wstring err;
    std::vector<std::wstring> warns;

    if (!CheckInputs(true, err, warns)) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }
    for (size_t i = 0; i < warns.size(); ++i) log_.Warn(warns[i]);

    SetState(AppState::EXECUTING);
    SetProgress(10);

    OperationContext ctx;
    ReadInputs(ctx);
    ctx.execution_mode = mode;
    ctx.operation_id = util::NewOperationId();
    ctx.encoding = file_info_.encoding;
    ctx.newline = file_info_.newline;

    std::vector<MatchResult> sel = GetSelectedMatches();
    ctx.selected_matches = sel;

    // 手順4～5: 対象ファイル再読み込みと検索結果の再検証
    std::wstring current;
    FileInfo cur_info;
    if (!FileManager::ReadText(target_path_, cfg_.encoding_mode, current, cur_info)) {
        ShowError(L"ファイルを読み取れません。");
        SetState(AppState::Error);
        return;
    }
    if (current != file_text_) {
        ShowWarn(L"対象ファイルが検索後に変更されています。\n"
                 L"安全のため処理を中断しました。再検索してください。");
        log_.Warn(L"対象ファイルが検索後に変更されています。再検索を要求します。");
        file_text_ = current;
        file_info_ = cur_info;
        matches_.clear();
        RefreshListView();
        SetState(AppState::FILE_SELECTED);
        return;
    }

    std::vector<std::wstring> blocks = MakeBlocksFor(sel, ctx, file_text_);
    std::wstring after = PreviewGenerator::BuildAfterTextEach(file_text_, sel, blocks);
    ApplyLogInsertion(after, sel);

    // スイッチ定義ヘッダの #include を追加する
    bool inc_added = false;
    int  inc_line = 0;
    if (ctx.add_switch_header_include && !ctx.switch_header_path.empty()) {
        if (!ApplyIncludeToText(after, target_path_, ctx, inc_added, inc_line)) {
            ShowError(L"#include の追加に失敗しました。");
            log_.Error(L"#include の追加に失敗しました。");
            SetState(AppState::Error);
            SetProgress(0);
            return;
        }
        if (inc_added)
            log_.Info(L"Include added at line " + std::to_wstring(inc_line) + L": " +
                      SwitchHeaderManager::MakeIncludePath(target_path_, ctx.switch_header_path));
    }

    if (mode == ExecutionMode::Real) {
        if (!Confirm(L"対象ファイルを書き換えます。よろしいですか？\n\n" +
                     target_path_ + L"\n置換件数: " +
                     std::to_wstring((int)sel.size()) + L" 件")) {
            log_.Info(L"ユーザーが実行をキャンセルしました。");
            SetState(AppState::PREVIEWED);
            SetProgress(0);
            return;
        }
    }

    SetProgress(30);

    ReplaceResult result;
    result.operation_id = ctx.operation_id;
    result.target_file_path = target_path_;
    result.before_text = file_text_;
    result.after_text = after;
    result.replaced_count = (int)sel.size();
    result.executed_at = util::NowDateTime();
    result.include_added = inc_added;
    result.include_line = inc_line;

    int warn_count = 0;
    for (size_t i = 0; i < sel.size(); ++i)
        if (sel[i].HasWarning()) ++warn_count;
    result.warning_count = warn_count;

    bool file_written = false;
    std::wstring backup_path;
    std::wstring header_backup;
    bool header_added = false;

    // ---- スイッチ定義ヘッダの生成／更新（本実行のみ。ドライランでは変更しない）----
    if (mode == ExecutionMode::Real && ctx.manage_switch_header) {
        std::vector<std::wstring> srcs;
        srcs.push_back(target_path_);
        if (!PrepareSwitchHeader(ctx, srcs, true, header_added, header_backup, err)) {
            ShowError(err);
            log_.Error(err);
            SetState(AppState::Error);
            SetProgress(0);
            return;
        }
        result.switch_header_path = ctx.switch_header_path;
        result.switch_header_updated = true;
        result.switch_header_added = header_added;
        log_.Info(std::wstring(header_added ? L"Switch registered: " : L"Switch header updated: ") +
                  ctx.switch_header_path);
    }

    if (mode == ExecutionMode::Real) {
        if (IsChecked(hChkBackup_)) {
            if (!BackupManager::CreateBackup(target_path_, BackupDirectory(),
                                             backup_path, err)) {
                ShowError(L"MSG007: " + err);
                log_.Error(err);
                SetState(AppState::Error);
                SetProgress(0);
                return;
            }
            result.backup_file_path = backup_path;
            log_.Info(L"Backup created: " + backup_path);
        }
        SetProgress(50);

        if (!FileManager::WriteTextAtomic(target_path_, after, cur_info.encoding,
                                          cur_info.has_bom, cur_info.newline, err)) {
            log_.Error(err);
            if (!backup_path.empty() && util::PathExists(backup_path)) {
                std::wstring rerr;
                if (FileManager::CopyFileTo(backup_path, target_path_, rerr)) {
                    log_.Warn(L"書き込み失敗のためバックアップから復旧しました。");
                    ShowError(L"MSG009: ファイルの更新に失敗しました。\n" + err +
                              L"\n\nバックアップから元の内容へ復旧しました。");
                } else {
                    ShowError(L"MSG009: ファイルの更新に失敗しました。\n" + err +
                              L"\n\n自動復旧にも失敗しました。バックアップを手動で復元してください:\n" +
                              backup_path);
                }
            } else {
                ShowError(L"MSG009: ファイルの更新に失敗しました。\n" + err);
            }
            SetState(AppState::Error);
            SetProgress(0);
            return;
        }
        file_written = true;
        log_.Info(L"File updated: " + target_path_);
    }

    SetProgress(70);

    std::wstring report_path;
    if (IsChecked(hChkReport_)) {
        std::wstring dir = ReportDirectory();
        report_path = DiffReportGenerator::BuildReportPath(
            dir, target_path_, util::TimestampFileName());
        std::wstring rerr;
        if (!DiffReportGenerator::Generate(report_path, ctx, sel, file_text_, after,
                                           result, rerr)) {
            log_.Warn(L"レポート出力に失敗しました: " + rerr);
            ShowWarn(L"レポート出力に失敗しました。\n" + rerr +
                     L"\n\nファイル更新自体は" + (file_written ? L"成功しています。" : L"行われていません。"));
            report_path.clear();
        } else {
            result.report_file_path = report_path;
            last_report_path_ = report_path;
            log_.Info(L"Report created: " + report_path);
        }
    }

    SetProgress(90);

    {
        std::vector<UndoRecord> undo_recs;
        if (file_written && !backup_path.empty()) {
            UndoRecord rec;
            rec.operation_id = ctx.operation_id;
            rec.target_file_path = target_path_;
            rec.backup_file_path = backup_path;
            rec.report_file_path = report_path;
            rec.switch_name = ctx.switch_name;
            rec.executed_at = util::NowDateTime();
            rec.valid = true;
            undo_recs.push_back(rec);
            last_backup_path_ = backup_path;
        }
        if (mode == ExecutionMode::Real && ctx.manage_switch_header &&
            !ctx.switch_header_path.empty()) {
            UndoRecord hrec;
            hrec.operation_id = ctx.operation_id;
            hrec.target_file_path = ctx.switch_header_path;
            hrec.backup_file_path = header_backup;
            hrec.remove_on_undo = header_backup.empty();   // 新規作成なら削除して戻す
            hrec.switch_name = ctx.switch_name;
            hrec.executed_at = util::NowDateTime();
            hrec.valid = true;
            undo_recs.push_back(hrec);
        }
        if (!undo_recs.empty()) {
            undo_.SetRecords(undo_recs);
            log_.Info(L"Undo information saved. Files: " +
                      std::to_wstring((int)undo_recs.size()));
        }
    }

    result.status = (warn_count > 0) ? L"Warning" : L"Success";
    if (mode == ExecutionMode::DryRun) result.status = L"Success";
    last_result_ = result;

    SetProgress(100);
    SetState(undo_.CanUndo() ? AppState::UNDO_AVAILABLE : AppState::COMPLETED);

    if (mode == ExecutionMode::DryRun) {
        log_.Info(L"Dry run completed. File was not modified.");
        std::wstring msg = L"ドライランが完了しました。ファイルは変更されていません。\n\n"
                           L"置換対象: " + std::to_wstring((int)sel.size()) + L" 件\n";
        if (!report_path.empty()) msg += L"レポート: " + report_path;
        ShowInfo(msg);
    } else {
        log_.Info(L"MSG008: ファイルの更新に成功しました。");
        std::wstring msg = L"MSG008: ファイルの更新に成功しました。\n\n"
                           L"置換件数: " + std::to_wstring((int)sel.size()) + L" 件\n";
        if (!backup_path.empty()) msg += L"バックアップ: " + backup_path + L"\n";
        if (result.switch_header_updated)
            msg += L"スイッチ定義ヘッダ: " + result.switch_header_path +
                   (header_added ? L"（スイッチを新規登録）" : L"（更新）") + L"\n";
        if (inc_added)
            msg += L"#include を " + std::to_wstring(inc_line) + L" 行目に追加しました。\n";
        if (!report_path.empty()) msg += L"レポート: " + report_path;
        ShowInfo(msg);
    }

    if (IsChecked(hChkOpenReport_) && !report_path.empty()) OpenPath(report_path);

    // 置換後も検索結果は保持する（プレビュータブの WinMerge 比較をそのまま使えるように）。
    // 検索結果を消すのは再走査・再検索を実行したときだけ。
    if (file_written) {
        // 自分で書き換えた内容を保持し、次回実行時に
        // 「検索後に変更されています」と誤判定して検索結果が消えるのを防ぐ。
        FileInfo fi2;
        std::wstring cur2;
        if (FileManager::ReadText(target_path_, cfg_.encoding_mode, cur2, fi2)) {
            file_text_ = cur2;
            file_info_ = fi2;
        }
        RefreshListView();
    }

    WriteExtraReportsAfterRun(false);
}

// -----------------------------------------------------------------------------
// 17.2.2 手順7-9 一括バックアップ・一括変換・統合レポート
// -----------------------------------------------------------------------------
void App::RunBatchOperation(ExecutionMode mode)
{
    std::wstring err;
    std::vector<std::wstring> warns;

    if (!CheckInputs(true, err, warns)) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }
    for (size_t i = 0; i < warns.size(); ++i) log_.Warn(warns[i]);

    OperationContext ctx;
    ReadInputs(ctx);
    ctx.execution_mode = mode;
    ctx.operation_id = util::NewOperationId();
    ctx.encoding = TextEncoding::UTF8;     // 一括処理ではファイルごとに異なる
    ctx.newline  = NewlineKind::LF;

    std::vector<MatchResult> sel = GetSelectedMatches();
    if (sel.empty()) {
        ShowError(L"置換対象が選択されていません。");
        SetState(AppState::SEARCHED);
        return;
    }
    ctx.selected_matches = sel;

    // matches_ と同じ並びのブロック配列を用意する（未選択要素は空）
    std::vector<std::wstring> blocks(matches_.size());
    for (size_t i = 0; i < matches_.size(); ++i) {
        if (!matches_[i].selected) continue;
        bool file_on = false;
        for (size_t k = 0; k < batch_entries_.size(); ++k)
            if (batch_entries_[k].path == matches_[i].file_path) {
                file_on = batch_entries_[k].selected;
                break;
            }
        if (!file_on) continue;
        std::wstring text;
        for (size_t k = 0; k < batch_entries_.size(); ++k)
            if (batch_entries_[k].path == matches_[i].file_path) {
                text = batch_entries_[k].text;
                break;
            }
        blocks[i] = MakeBlockFor(matches_[i], ctx, text);
    }

    // 対象ファイル数を集計
    int target_files = 0;
    for (size_t i = 0; i < batch_entries_.size(); ++i)
        if (batch_entries_[i].selected && batch_entries_[i].selected_count > 0) ++target_files;

    if (mode == ExecutionMode::Real) {
        if (!Confirm(L"対象フォルダ内のファイルを一括で書き換えます。よろしいですか？\n\n" +
                     batch_folder_ + L"\n対象ファイル: " + std::to_wstring(target_files) +
                     L" 件\n置換件数: " + std::to_wstring((int)sel.size()) + L" 件\n\n" +
                     L"バックアップを作成してから実行します。")) {
            log_.Info(L"ユーザーが一括実行をキャンセルしました。");
            SetState(AppState::PREVIEWED);
            SetProgress(0);
            return;
        }
    }

    // ---- スイッチ定義ヘッダの生成／更新（本実行のみ。ドライランでは変更しない）----
    std::wstring header_backup;
    bool header_added = false;
    if (mode == ExecutionMode::Real && ctx.manage_switch_header) {
        std::vector<std::wstring> srcs;
        for (size_t i = 0; i < batch_entries_.size() && srcs.size() < 21; ++i) {
            if (!batch_entries_[i].selected || batch_entries_[i].selected_count == 0) continue;
            srcs.push_back(batch_entries_[i].path);
        }
        if (!PrepareSwitchHeader(ctx, srcs, true, header_added, header_backup, err)) {
            ShowError(err);
            log_.Error(err);
            SetState(AppState::Error);
            SetProgress(0);
            return;
        }
        log_.Info(std::wstring(header_added ? L"Switch registered: " : L"Switch header updated: ") +
                  ctx.switch_header_path + L"（対象 " + std::to_wstring((int)srcs.size()) + L" ファイル）");
    }

    SetState(AppState::EXECUTING);
    SetProgress(5);
    log_.Info(std::wstring(mode == ExecutionMode::DryRun ? L"Batch dry run started." :
                                                          L"Batch execution started."));

    BatchExecuteResult br;
    std::vector<UndoRecord> undo_recs;

    BatchProgressFn prog = [this](int done, int total, const std::wstring& cur) {
        int pct = (total > 0) ? (int)((done * 85) / total) + 5 : 5;
        SetProgress(pct);
        if (!cur.empty())
            SetStatus(L"処理中 (" + std::to_wstring(done) + L"/" + std::to_wstring(total) +
                      L"): " + util::GetFileNameOf(cur));
    };

    if (!BatchProcessor::ExecuteBatch(ctx, cfg_.encoding_mode, batch_entries_, matches_,
                                      blocks, BackupDirectory(), prog, br, undo_recs, err)) {
        ShowError(err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }

    br.switch_header_path = ctx.switch_header_path;
    br.switch_header_updated = (mode == ExecutionMode::Real && ctx.manage_switch_header);
    br.switch_header_added = header_added;

    SetProgress(90);
    RefreshFileList();
    RefreshListView();

    // ---- 手順9: 統合レポート出力 ----
    std::vector<ReportFileSection> sections;
    for (size_t ei = 0; ei < batch_entries_.size(); ++ei) {
        BatchFileEntry& e = batch_entries_[ei];
        if (!e.selected) continue;

        std::vector<MatchResult> fm;
        std::vector<std::wstring> fb;
        for (size_t mi = 0; mi < matches_.size(); ++mi) {
            if (matches_[mi].file_path != e.path) continue;
            if (!matches_[mi].selected) continue;
            fm.push_back(matches_[mi]);
            fb.push_back(blocks[mi]);
        }
        if (fm.empty()) continue;

        ReportFileSection s;
        s.file_path = e.path;
        s.matches = fm;
        s.before_text = e.text;
        s.after_text = PreviewGenerator::BuildAfterTextEach(e.text, fm, fb);
        for (size_t k = 0; k < br.files.size(); ++k)
            if (br.files[k].file_path == e.path) s.result = br.files[k];
        s.encoding_name = EncodingDetector::EncodingName(e.info.encoding);
        s.newline_name = (e.info.newline == NewlineKind::CRLF ? L"CRLF" :
                          (e.info.newline == NewlineKind::CR ? L"CR" : L"LF"));
        s.include_added = e.include_added;
        s.include_line = e.include_line;
        sections.push_back(s);
    }

    std::wstring report_path;
    if (IsChecked(hChkReport_) && !sections.empty()) {
        report_path = DiffReportGenerator::BuildIntegratedReportPath(
            ReportDirectory(), batch_folder_, util::TimestampFileName());
        std::wstring rerr;
        if (!DiffReportGenerator::GenerateIntegrated(report_path, ctx, sections, br, rerr)) {
            log_.Warn(L"統合レポート出力に失敗しました: " + rerr);
            ShowWarn(L"統合レポート出力に失敗しました。\n" + rerr +
                     L"\n\nファイル更新自体は" +
                     (mode == ExecutionMode::Real ? L"完了しています。" : L"行われていません。"));
            report_path.clear();
        } else {
            br.report_file_path = report_path;
            last_report_path_ = report_path;
            log_.Info(L"Integrated report created: " + report_path);
        }
    }

    // ---- Undo 情報保存（操作単位で全ファイルを復元できるようにする）----
    if (mode == ExecutionMode::Real && ctx.manage_switch_header &&
        !ctx.switch_header_path.empty()) {
        UndoRecord hrec;
        hrec.operation_id = ctx.operation_id;
        hrec.target_file_path = ctx.switch_header_path;
        hrec.backup_file_path = header_backup;
        hrec.remove_on_undo = header_backup.empty();
        hrec.switch_name = ctx.switch_name;
        hrec.executed_at = util::NowDateTime();
        hrec.valid = true;
        undo_recs.push_back(hrec);
    }
    if (mode == ExecutionMode::Real && !undo_recs.empty()) {
        for (size_t i = 0; i < undo_recs.size(); ++i)
            undo_recs[i].report_file_path = report_path;
        undo_.SetRecords(undo_recs);
        log_.Info(L"Undo information saved. Files: " +
                  std::to_wstring((int)undo_recs.size()));
    }

    last_batch_result_ = br;
    SetProgress(100);
    SetState(undo_.CanUndo() ? AppState::UNDO_AVAILABLE : AppState::COMPLETED);

    log_.Info(L"Batch completed. Replaced: " + std::to_wstring(br.replaced_count) +
              L" / Failed: " + std::to_wstring(br.error_count) +
              L" / Skipped: " + std::to_wstring(br.skipped_count));

    wchar_t buf[512];
    if (mode == ExecutionMode::DryRun) {
        swprintf(buf, 512,
                 L"一括ドライランが完了しました。ファイルは変更されていません。\n\n"
                 L"対象ファイル: %d 件\n置換件数: %d 件\n",
                 (int)sections.size(), br.replaced_count);
    } else {
        swprintf(buf, 512,
                 L"一括処理が完了しました。\n\n"
                 L"成功: %d 件 / 失敗: %d 件 / スキップ: %d 件\n置換件数: %d 件\n",
                 br.replaced_count, br.error_count, br.skipped_count, br.replaced_count);
    }
    std::wstring msg = buf;
    if (br.switch_header_updated)
        msg += L"\nスイッチ定義ヘッダ: " + br.switch_header_path +
               (header_added ? L"（スイッチを新規登録）" : L"（更新）");
    if (!report_path.empty()) msg += L"\n統合レポート: " + report_path;
    if (br.error_count > 0) {
        msg += L"\n\n失敗したファイルがあります。一括処理タブの「状態」列と"
               L"統合レポートを確認してください。";
        ShowWarn(msg);
    } else {
        ShowInfo(msg);
    }

    if (IsChecked(hChkOpenReport_) && !report_path.empty()) OpenPath(report_path);

    // 実行後も検索結果は保持する（プレビュータブの WinMerge 比較をそのまま使えるように）。
    // 検索結果を消すのは再走査・再検索を実行したときだけ。
    if (mode == ExecutionMode::Real) {
        RefreshListView();
    }

    WriteExtraReportsAfterRun(true);
}


// -----------------------------------------------------------------------------
// スイッチ定義ヘッダの一元管理
// -----------------------------------------------------------------------------
std::wstring App::SwitchHeaderBaseDir() const
{
    if (IsFolderMode()) {
        std::wstring f = util::Trim(GetEditText(hFolderEdit_));
        if (!f.empty()) return util::MakeAbsolute(f);
    }
    std::wstring t = util::Trim(GetEditText(hTargetEdit_));
    if (!t.empty()) {
        std::wstring abs = util::MakeAbsolute(t);
        std::wstring d = util::GetDirectoryOf(abs);
        if (!d.empty()) return d;
    }
    return util::ModuleDirectory();
}

std::wstring App::CurrentSwitchHeaderPath() const
{
    std::wstring name = util::Trim(GetEditText(hEditSwHeaderName_));
    if (name.empty()) name = SwitchHeaderManager::DefaultHeaderName();
    // フルパスで入力された場合はそのまま使う
    if (!util::GetDirectoryOf(name).empty())
        return util::MakeAbsolute(name);
    return SwitchHeaderManager::ResolveHeaderPath(
        SwitchHeaderBaseDir(), util::Trim(GetEditText(hEditSwHeaderDir_)), name);
}

void App::RefreshResolvedHeaderPath()
{
    SetEditText(hSwHeaderEdit_, CurrentSwitchHeaderPath());
}

void App::RefreshSwitchList()
{
    if (!hListSwitches_) return;
    suppress_events_ = true;
    ::SendMessageW(hListSwitches_, LVM_DELETEALLITEMS, 0, 0);

    for (size_t i = 0; i < sw_header_info_.entries.size(); ++i) {
        const SwitchEntry& e = sw_header_info_.entries[i];

        std::wstring state = e.enabled ? L"有効（新コード）" : L"無効（旧コード）";
        std::wstring files;
        for (size_t k = 0; k < e.files.size(); ++k) {
            if (k) files += L", ";
            files += e.files[k];
        }
        std::wstring desc;
        if (!e.comment.empty()) desc += e.comment;
        if (!e.summary.empty()) desc += (desc.empty() ? L"" : L" | ") + e.summary;
        if (!files.empty()) desc += L"  [" + files + L"]";
        if (!e.updated_at.empty()) desc += L"  " + e.updated_at;

        LVITEMW it;
        ::ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.iSubItem = 0;
        std::wstring no = std::to_wstring((int)i + 1);
        it.pszText = (LPWSTR)no.c_str();
        int row = (int)::SendMessageW(hListSwitches_, LVM_INSERTITEMW, 0, (LPARAM)&it);

        auto setSub = [&](int sub, const std::wstring& text) {
            LVITEMW si;
            ::ZeroMemory(&si, sizeof(si));
            si.iItem = row;
            si.iSubItem = sub;
            si.mask = LVIF_TEXT;
            si.pszText = (LPWSTR)text.c_str();
            ::SendMessageW(hListSwitches_, LVM_SETITEMW, 0, (LPARAM)&si);
        };
        setSub(1, e.name);
        setSub(2, state);
        setSub(3, files);
        setSub(4, desc);
    }

    wchar_t buf[512];
    if (!sw_header_info_.exists) {
        swprintf(buf, 512, L"ヘッダ未作成: %s", sw_header_info_.path.c_str());
    } else if (sw_header_info_.entries.empty()) {
        swprintf(buf, 512, L"%s（スイッチ未登録）", sw_header_info_.path.c_str());
    } else {
        int on = 0;
        for (size_t i = 0; i < sw_header_info_.entries.size(); ++i)
            if (sw_header_info_.entries[i].enabled) ++on;
        swprintf(buf, 512, L"%s / スイッチ %d 件（有効 %d / 無効 %d）",
                 sw_header_info_.path.c_str(),
                 (int)sw_header_info_.entries.size(), on,
                 (int)sw_header_info_.entries.size() - on);
    }
    SetEditText(hLblSwInfo_, buf);
    suppress_events_ = false;
}

void App::DoSwitchHeaderLoad()
{
    std::wstring path = CurrentSwitchHeaderPath();
    RefreshResolvedHeaderPath();

    if (!util::PathExists(path)) {
        sw_header_info_ = SwitchHeaderInfo();
        sw_header_info_.path = path;
        RefreshSwitchList();
        log_.Info(L"スイッチ定義ヘッダは未作成です: " + path);
        return;
    }
    if (!SwitchHeaderManager::Load(path, sw_header_info_)) {
        ShowError(L"スイッチ定義ヘッダを読み込めません: " + path);
        log_.Error(L"Failed to load switch header: " + path);
        return;
    }
    RefreshSwitchList();
    log_.Info(L"スイッチ定義ヘッダを読み込みました: " + path +
              L"（" + std::to_wstring((int)sw_header_info_.entries.size()) + L" 件）");
}

void App::DoSwitchHeaderBrowse()
{
    std::wstring initial = CurrentSwitchHeaderPath();
    if (initial.empty()) initial = util::ModuleDirectory();

    wchar_t buf[MAX_PATH * 4];
    ::ZeroMemory(buf, sizeof(buf));
    lstrcpynW(buf, initial.c_str(), MAX_PATH * 4);

    OPENFILENAMEW ofn;
    ::ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hWnd_;
    ofn.lpstrFilter = L"C/C++ ヘッダ (*.h)\0*.h\0すべてのファイル (*.*)\0*.*\0\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH * 4;
    ofn.lpstrTitle  = L"スイッチ定義ヘッダを選択";
    ofn.Flags       = OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!::GetOpenFileNameW(&ofn)) return;

    std::wstring chosen = util::MakeAbsolute(buf);
    SetEditText(hEditSwHeaderName_, util::GetFileNameOf(chosen));
    SetEditText(hEditSwHeaderDir_, util::GetDirectoryOf(chosen));
    RefreshResolvedHeaderPath();
    DoSwitchHeaderLoad();
}

void App::DoSwitchHeaderDirBrowse()
{
    BROWSEINFOW bi;
    ::ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = hWnd_;
    bi.lpszTitle = L"スイッチ定義ヘッダの出力先フォルダを選択してください";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

    LPITEMIDLIST pidl = ::SHBrowseForFolderW(&bi);
    if (!pidl) return;

    wchar_t path[MAX_PATH * 2];
    path[0] = 0;
    if (::SHGetPathFromIDListW(pidl, path)) {
        SetEditText(hEditSwHeaderDir_, path);
        RefreshResolvedHeaderPath();
        DoSwitchHeaderLoad();
    }
    ::CoTaskMemFree(pidl);
}

bool App::PrepareSwitchHeader(const OperationContext& ctx,
                              const std::vector<std::wstring>& source_files,
                              bool backup_header,
                              bool& out_added,
                              std::wstring& out_backup,
                              std::wstring& out_error)
{
    out_added = false;
    out_backup.clear();
    out_error.clear();

    if (!ctx.manage_switch_header) return true;
    if (ctx.switch_header_path.empty()) {
        out_error = L"スイッチ定義ヘッダのパスが決定できません。";
        return false;
    }

    // 既存ヘッダをバックアップしてから更新する（19.1）
    if (backup_header && util::PathExists(ctx.switch_header_path)) {
        std::wstring berr;
        if (!BackupManager::CreateBackup(ctx.switch_header_path, BackupDirectory(),
                                         out_backup, berr)) {
            out_error = L"スイッチ定義ヘッダのバックアップに失敗しました: " + berr;
            return false;
        }
    }

    SwitchHeaderInfo info;
    if (!SwitchHeaderManager::RegisterSwitch(ctx.switch_header_path, ctx.switch_name,
                                             ctx.switch_memo, ctx.old_code, ctx.new_code,
                                             source_files, info, out_added, out_error)) {
        return false;
    }
    sw_header_info_ = info;
    RefreshSwitchList();
    return true;
}

bool App::ApplyIncludeToText(std::wstring& text,
                             const std::wstring& source_path,
                             const OperationContext& ctx,
                             bool& out_added,
                             int& out_line)
{
    out_added = false;
    out_line = 0;
    if (!ctx.add_switch_header_include) return true;
    if (ctx.switch_header_path.empty()) return true;

    std::wstring inc = SwitchHeaderManager::MakeIncludePath(source_path, ctx.switch_header_path);
    std::wstring err;
    return SwitchHeaderManager::EnsureInclude(text, inc, out_added, out_line, err);
}

void App::DoSwitchCreateNow()
{
    std::wstring sw = util::Trim(GetEditText(hSwitchEdit_));
    std::wstring oc = util::NormalizeNewlines(GetEditText(hOldEdit_));
    std::wstring nc = util::NormalizeNewlines(GetEditText(hNewEdit_));

    ValidationResult vs = ValidationManager::ValidateSwitchName(sw);
    if (!vs.ok) { ShowError(vs.error); return; }
    ValidationResult vo = ValidationManager::ValidateOldCode(oc);
    if (!vo.ok) { ShowError(vo.error); return; }

    std::wstring hn = util::Trim(GetEditText(hEditSwHeaderName_));
    if (hn.empty()) hn = SwitchHeaderManager::DefaultHeaderName();
    std::wstring herr;
    if (!SwitchHeaderManager::ValidateHeaderName(hn, herr)) { ShowError(herr); return; }

    std::wstring path = CurrentSwitchHeaderPath();
    std::vector<std::wstring> srcs;
    std::wstring t = util::Trim(GetEditText(hTargetEdit_));
    if (!t.empty()) srcs.push_back(util::MakeAbsolute(t));

    // 既存ヘッダは退避してから更新する
    std::wstring backup;
    if (util::PathExists(path)) {
        std::wstring berr;
        if (!BackupManager::CreateBackup(path, BackupDirectory(), backup, berr)) {
            ShowError(L"スイッチ定義ヘッダのバックアップに失敗しました。\n" + berr);
            return;
        }
    }

    SwitchHeaderInfo info;
    bool added = false;
    if (!SwitchHeaderManager::RegisterSwitch(path, sw, util::Trim(GetEditText(hEditSwMemo_)),
                                             oc, nc, srcs, info, added, herr)) {
        ShowError(L"スイッチ定義ヘッダの更新に失敗しました。\n" + herr);
        log_.Error(herr);
        return;
    }
    sw_header_info_ = info;
    RefreshSwitchList();
    log_.Info(std::wstring(added ? L"Switch registered: " : L"Switch header updated: ") + path);

    std::wstring msg = L"スイッチ定義ヘッダを" + std::wstring(added ? L"作成しました。" : L"更新しました。") +
                       L"\n\n" + path + L"\n\nスイッチ: " + sw +
                       (added ? L"（新規登録・既定は無効）" : L"（既存を更新）");
    if (!backup.empty()) msg += L"\nバックアップ: " + backup;
    ShowInfo(msg);
}

void App::DoSwitchEnable()
{
    int sel = ListView_GetNextItem(hListSwitches_, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)sw_header_info_.entries.size()) {
        ShowInfo(L"有効化するスイッチを一覧から選択してください。");
        return;
    }
    std::wstring name = sw_header_info_.entries[(size_t)sel].name;
    if (!Confirm(L"スイッチ '" + name + L"' を有効にします。\n\n"
                 L"このスイッチを参照しているソースでは新コードが使われるようになります。\n"
                 L"よろしいですか？")) return;

    SwitchHeaderInfo info;
    std::wstring err;
    if (!SwitchHeaderManager::SetEnabled(CurrentSwitchHeaderPath(), name, true, info, err)) {
        ShowError(L"有効化に失敗しました。\n" + err);
        log_.Error(err);
        return;
    }
    sw_header_info_ = info;
    RefreshSwitchList();
    log_.Info(L"スイッチを有効化しました: " + name);
}

void App::DoSwitchDisable()
{
    int sel = ListView_GetNextItem(hListSwitches_, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)sw_header_info_.entries.size()) {
        ShowInfo(L"無効化するスイッチを一覧から選択してください。");
        return;
    }
    std::wstring name = sw_header_info_.entries[(size_t)sel].name;

    SwitchHeaderInfo info;
    std::wstring err;
    if (!SwitchHeaderManager::SetEnabled(CurrentSwitchHeaderPath(), name, false, info, err)) {
        ShowError(L"無効化に失敗しました。\n" + err);
        log_.Error(err);
        return;
    }
    sw_header_info_ = info;
    RefreshSwitchList();
    log_.Info(L"スイッチを無効化しました: " + name);
}

void App::DoSwitchRemove()
{
    int sel = ListView_GetNextItem(hListSwitches_, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= (int)sw_header_info_.entries.size()) {
        ShowInfo(L"削除するスイッチを一覧から選択してください。");
        return;
    }
    std::wstring name = sw_header_info_.entries[(size_t)sel].name;
    if (!Confirm(L"スイッチ '" + name + L"' をヘッダの管理領域から削除します。\n\n"
                 L"ソース側の #ifdef は変更されません。削除すると未定義のままになります。\n"
                 L"よろしいですか？")) return;

    SwitchHeaderInfo info;
    std::wstring err;
    if (!SwitchHeaderManager::RemoveSwitch(CurrentSwitchHeaderPath(), name, info, err)) {
        ShowError(L"削除に失敗しました。\n" + err);
        log_.Error(err);
        return;
    }
    sw_header_info_ = info;
    RefreshSwitchList();
    log_.Info(L"スイッチを削除しました: " + name);
}

void App::DoSwitchOpen()
{
    std::wstring path = CurrentSwitchHeaderPath();
    if (!util::PathExists(path)) {
        ShowInfo(L"スイッチ定義ヘッダはまだ作成されていません。\n\n" + path);
        return;
    }
    OpenPath(path);
}

// -----------------------------------------------------------------------------
// 9.6 Undo 処理フロー
// -----------------------------------------------------------------------------
void App::DoUndo()
{
    std::wstring err;
    if (!undo_.CanUndo()) {
        ShowInfo(L"元に戻せる操作がありません。");
        return;
    }
    int file_count = undo_.FileCount();

    std::wstring msg = L"直前の変更を元に戻します。よろしいですか？\n\n";
    msg += L"Undo 履歴: " + std::to_wstring(undo_.HistorySize()) +
           L" 世代（この操作で 1 世代戻ります）\n";
    if (file_count > 1) {
        msg += L"対象ファイル: " + std::to_wstring(file_count) + L" 件（一括処理）\n";
        msg += L"対象フォルダ: " + undo_.Record().target_file_path + L" ほか\n";
    } else {
        msg += L"対象ファイル: " + undo_.Record().target_file_path + L"\n";
        msg += L"復元元: " + undo_.Record().backup_file_path;
    }
    if (!Confirm(msg)) return;

    SetState(AppState::EXECUTING);
    log_.Info(L"Undo started.");

    std::vector<std::wstring> sidecars;
    if (!undo_.Undo(err, sidecars)) {
        ShowError(L"Undoに失敗しました。\n" + err);
        log_.Error(err);
        SetState(AppState::Error);
        return;
    }

    log_.Info(L"Undo completed. Restored files: " + std::to_wstring((int)sidecars.size()));
    for (size_t i = 0; i < sidecars.size(); ++i)
        log_.Info(L"  現在の内容を退避しました: " + sidecars[i]);

    // 再読み込み
    matches_.clear();
    if (IsFolderMode()) {
        batch_entries_.clear();
        RefreshFileList();
    } else {
        target_path_ = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
        FileManager::ReadText(target_path_, cfg_.encoding_mode, file_text_, file_info_);
    }
    RefreshListView();
    SetState(AppState::FILE_SELECTED);
    {
        int left = undo_.HistorySize();
        std::wstring m = L"MSG010: Undoが完了しました。";
        if (left > 0)
            m += L"\nさらに元に戻せる操作が " + std::to_wstring(left) + L" 世代あります。";
        ShowInfo(m);
    }
}

// -----------------------------------------------------------------------------
// 5.7.2 HTML 表示
// -----------------------------------------------------------------------------
void App::DoOpenHtml()
{
    // 比較範囲が選ばれていれば、その範囲のレポートを作る（無ければ新規作成）
    {
        std::wstring fid, tid;
        if (GetPairIds(hCmbPairFrom_, hCmbPairTo_, fid, tid)) {
            BuildPairReport(fid, tid, L"preview");
            return;
        }
    }
    std::vector<MatchResult> sel = GetSelectedMatches();
    if (matches_.empty() || sel.empty()) {
        ShowError(L"置換対象が選択されていません。先に検索と対象選択を行ってください。");
        return;
    }

    OperationContext ctx;
    ReadInputs(ctx);
    ctx.selected_matches = sel;
    ctx.execution_mode = ExecutionMode::DryRun;
    ctx.operation_id = util::NewOperationId();

    std::wstring path;
    std::wstring err;

    if (IsFolderMode()) {
        // 一括処理の統合レポートをプレビュー用に生成する
        std::vector<std::wstring> blocks(matches_.size());
        for (size_t i = 0; i < matches_.size(); ++i) {
            if (!matches_[i].selected) continue;
            std::wstring text;
            for (size_t k = 0; k < batch_entries_.size(); ++k)
                if (batch_entries_[k].path == matches_[i].file_path) {
                    text = batch_entries_[k].text;
                    break;
                }
            blocks[i] = MakeBlockFor(matches_[i], ctx, text);
        }

        std::vector<ReportFileSection> sections;
        for (size_t ei = 0; ei < batch_entries_.size(); ++ei) {
            BatchFileEntry& e = batch_entries_[ei];
            if (!e.selected) continue;
            std::vector<MatchResult> fm;
            std::vector<std::wstring> fb;
            for (size_t mi = 0; mi < matches_.size(); ++mi) {
                if (matches_[mi].file_path != e.path || !matches_[mi].selected) continue;
                fm.push_back(matches_[mi]);
                fb.push_back(blocks[mi]);
            }
            if (fm.empty()) continue;

            ReportFileSection s;
            s.file_path = e.path;
            s.matches = fm;
            s.before_text = e.text;
            s.after_text = PreviewGenerator::BuildAfterTextEach(e.text, fm, fb);
            s.result.status = L"DryRun";
            s.result.replaced_count = (int)fm.size();
            s.encoding_name = EncodingDetector::EncodingName(e.info.encoding);
            s.newline_name = (e.info.newline == NewlineKind::CRLF ? L"CRLF" :
                              (e.info.newline == NewlineKind::CR ? L"CR" : L"LF"));
            sections.push_back(s);
        }

        BatchExecuteResult br;
        br.operation_id = ctx.operation_id;
        br.executed_at = util::NowDateTime();
        br.folder = batch_folder_;
        br.status = L"Success";
        for (size_t i = 0; i < sections.size(); ++i)
            br.replaced_count += sections[i].result.replaced_count;

        path = DiffReportGenerator::BuildIntegratedReportPath(
            ReportDirectory(), batch_folder_, util::TimestampFileName());
        if (!DiffReportGenerator::GenerateIntegrated(path, ctx, sections, br, err)) {
            ShowError(L"統合レポートの生成に失敗しました。\n" + err);
            log_.Error(err);
            return;
        }
    } else {
        ctx.encoding = file_info_.encoding;
        ctx.newline = file_info_.newline;
        std::vector<std::wstring> blocks = MakeBlocksFor(sel, ctx, file_text_);
        std::wstring after = PreviewGenerator::BuildAfterTextEach(file_text_, sel, blocks);
    ApplyLogInsertion(after, sel);

        ReplaceResult result;
        result.operation_id = ctx.operation_id;
        result.target_file_path = target_path_;
        result.before_text = file_text_;
        result.after_text = after;
        result.replaced_count = (int)sel.size();
        result.executed_at = util::NowDateTime();
        result.status = L"Success";

        path = DiffReportGenerator::BuildReportPath(
            ReportDirectory(), target_path_, util::TimestampFileName());
        if (!DiffReportGenerator::Generate(path, ctx, sel, file_text_, after, result, err)) {
            ShowError(L"レポートの生成に失敗しました。\n" + err);
            log_.Error(err);
            return;
        }
    }

    last_report_path_ = path;
    log_.Info(L"Report created (preview): " + path);
    OpenPath(path);
}


// -----------------------------------------------------------------------------
// AI 連携（変更仕様書 / 検査仕様書の生成）
// -----------------------------------------------------------------------------
AiSettings App::ReadAiSettings() const
{
    AiSettings st;
    int sel = (int)::SendMessageW(hCmbAiProvider_, CB_GETCURSEL, 0, 0);
    st.provider = (sel == 1) ? AiProvider::OpenAiCompat : AiProvider::Genspark;
    st.api_key  = util::Trim(GetEditText(hEditAiKey_));
    st.endpoint = util::Trim(GetEditText(hEditAiEndpoint_));
    if (st.endpoint.empty())
        st.endpoint = (st.provider == AiProvider::OpenAiCompat)
                      ? AiClient::DefaultOpenAiEndpoint() : AiClient::DefaultEndpoint();
    st.model    = util::Trim(GetEditText(hEditAiModel_));
    st.task_type = cfg_.ai_task_type.empty() ? AiClient::DefaultTaskType() : cfg_.ai_task_type;
    int tmo = ::_wtoi(util::Trim(GetEditText(hEditAiTimeout_)).c_str());
    if (tmo < 30) tmo = 30;
    if (tmo > 3600) tmo = 3600;
    st.timeout_sec = tmo;
    return st;
}

SpecInput App::BuildSpecInput() const
{
    SpecInput in;
    in.system_name  = util::Trim(GetEditText(hEditSysName_));
    in.overview     = util::Trim(GetEditText(hEditSysOverview_));
    in.purpose      = util::Trim(GetEditText(hEditSysPurpose_));
    in.modification = util::Trim(GetEditText(hEditSysMod_));
    in.extra        = util::Trim(GetEditText(hEditSysExtra_));
    in.switch_name  = util::Trim(GetEditText(hSwitchEdit_));
    in.old_code     = util::NormalizeNewlines(GetEditText(hOldEdit_));
    in.new_code     = util::NormalizeNewlines(GetEditText(hNewEdit_));
    in.created_at   = util::NowDateTime();

    if (IsFolderMode()) {
        in.target_summary = L"フォルダ " + util::Trim(GetEditText(hFolderEdit_)) +
                            L"（走査 " + std::to_wstring((int)batch_entries_.size()) +
                            L" ファイル / ヒット " + std::to_wstring((int)matches_.size()) + L" 件）";
    } else {
        in.target_summary = util::Trim(GetEditText(hTargetEdit_));
    }

    // スイッチ定義ヘッダの内容（あれば抜粋してプロンプトに含める）
    std::wstring hp = CurrentSwitchHeaderPath();
    if (util::PathExists(hp)) {
        std::wstring htext;
        FileInfo hfi;
        if (FileManager::ReadText(hp, L"auto", htext, hfi)) {
            if (htext.size() > 4000) htext = htext.substr(0, 4000) + L"\n...(以下省略)";
            in.switch_header_text = htext;
        }
    }
    return in;
}

void App::PumpMessages()
{
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { ::PostQuitMessage((int)msg.wParam); return; }
        if (hWnd_ && ::IsDialogMessageW(hWnd_, &msg)) continue;
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

void App::DoGenerateSpec(int kind) { GenerateSpecs(kind == SPEC_CHANGE, kind == SPEC_INSPECTION); }
void App::DoGenerateSpecBoth()      { GenerateSpecs(true, true); }

void App::DoOpenSpecReport()
{
    if (last_spec_report_path_.empty() || !util::PathExists(last_spec_report_path_)) {
        ShowWarn(L"生成済みの仕様書レポートがありません。先に「変更仕様書を生成」等を実行してください。");
        return;
    }
    OpenPath(last_spec_report_path_);
}

// API 種別ごとの入力欄の読み書き
void App::LoadAiFieldsFromConfig(int provider)
{
    if (provider == 1) {
        SetEditText(hEditAiKey_,      cfg_.ai_openai_key);
        SetEditText(hEditAiEndpoint_, cfg_.ai_openai_endpoint.empty()
                                      ? AiClient::DefaultOpenAiEndpoint()
                                      : cfg_.ai_openai_endpoint);
        SetEditText(hEditAiModel_,    cfg_.ai_openai_model);
    } else {
        SetEditText(hEditAiKey_,      cfg_.ai_api_key);
        SetEditText(hEditAiEndpoint_, cfg_.ai_endpoint.empty()
                                      ? AiClient::DefaultEndpoint()
                                      : cfg_.ai_endpoint);
        SetEditText(hEditAiModel_,    cfg_.ai_model);
    }
}

void App::SaveAiFieldsToConfig(int provider)
{
    std::wstring key = util::Trim(GetEditText(hEditAiKey_));
    std::wstring ep  = util::Trim(GetEditText(hEditAiEndpoint_));
    std::wstring md  = util::Trim(GetEditText(hEditAiModel_));
    if (provider == 1) {
        cfg_.ai_openai_key = key;
        cfg_.ai_openai_endpoint = ep.empty() ? AiClient::DefaultOpenAiEndpoint() : ep;
        cfg_.ai_openai_model = md;
    } else {
        cfg_.ai_api_key = key;
        cfg_.ai_endpoint = ep.empty() ? AiClient::DefaultEndpoint() : ep;
        cfg_.ai_model = md;
    }
}

// API 種別の切り替え: 現在の入力を保存してから、切り替え先の設定を読み込む
void App::DoAiProviderChanged()
{
    int oldp = cfg_.ai_provider;
    int newp = (int)::SendMessageW(hCmbAiProvider_, CB_GETCURSEL, 0, 0);
    if (newp < 0 || newp > 1) newp = 0;
    if (oldp == newp) return;
    SaveAiFieldsToConfig(oldp);
    cfg_.ai_provider = newp;
    LoadAiFieldsFromConfig(newp);
    log_.Info(newp == 1 ? L"AI 連携: OpenAI 互換 API に切り替えました。"
                        : L"AI 連携: Genspark API に切り替えました。");
    SetStatus(newp == 1
              ? L"AI 連携: OpenAI 互換 API（GPT / Azure OpenAI / Ollama など）を使用します。"
              : L"AI 連携: Genspark API を使用します。");
}


// =============================================================================
// 後続版: 検索条件の拡張 / 結果エクスポート / WinMerge 連携 / 追加レポート
// =============================================================================
bool App::SearchOptionsEnabled() const
{
    return IsChecked(hChkSimilar_) || IsChecked(hChkLineRange_) || IsChecked(hChkFuncScope_);
}

int App::ReadSimilarThresholdFromUi() const
{
    int v = ParseIntOr(util::Trim(GetEditText(hEditSimThreshold_)), 70);
    if (v < 1)   v = 1;
    if (v > 100) v = 100;
    return v;
}

int App::ReadLineFromFromUi() const
{
    int v = ParseIntOr(util::Trim(GetEditText(hEditLineFrom_)), 1);
    return (v < 1) ? 1 : v;
}

int App::ReadLineToFromUi() const
{
    int v = ParseIntOr(util::Trim(GetEditText(hEditLineTo_)), 0);
    return (v < 0) ? 0 : v;
}

int App::ReadUndoMaxFromUi() const
{
    int v = ParseIntOr(util::Trim(GetEditText(hEditUndoMax_)), 10);
    if (v < 1)   v = 1;
    if (v > 100) v = 100;
    return v;
}

std::wstring App::ReadEncodingModeFromUi() const
{
    int sel = (int)::SendMessageW(hCmbEncoding_, CB_GETCURSEL, 0, 0);
    switch (sel) {
    case 1: return L"utf8";
    case 2: return L"utf8bom";
    case 3: return L"sjis";
    case 4: return L"eucjp";
    default: return L"auto";
    }
}

void App::LoadSearchOptionsFromConfig()
{
    SetChecked(hChkSimilar_, cfg_.search_similar);
    SetEditText(hEditSimThreshold_, std::to_wstring(cfg_.search_similar_threshold));
    SetChecked(hChkLineRange_, cfg_.search_line_range);
    SetEditText(hEditLineFrom_, std::to_wstring(cfg_.search_line_from));
    SetEditText(hEditLineTo_, std::to_wstring(cfg_.search_line_to));
    SetChecked(hChkFuncScope_, cfg_.search_function);
    SetEditText(hEditFuncName_, cfg_.search_function_name);
    SetChecked(hChkReportMd_, cfg_.report_markdown);
    SetChecked(hChkReportJson_, cfg_.report_json);
    SetEditText(hEditWinMerge_, cfg_.winmerge_path);
    SetEditText(hEditUndoMax_, std::to_wstring(cfg_.undo_max_generations));

    std::wstring m = util::ToUpper(cfg_.encoding_mode);
    int sel = 0;
    if      (m == L"UTF8" || m == L"UTF-8")                          sel = 1;
    else if (m == L"UTF8BOM")                                        sel = 2;
    else if (m == L"SJIS" || m == L"SHIFT_JIS" || m == L"CP932")     sel = 3;
    else if (m == L"EUCJP" || m == L"EUC-JP")                        sel = 4;
    ::SendMessageW(hCmbEncoding_, CB_SETCURSEL, sel, 0);

    undo_.SetMaxGenerations(cfg_.undo_max_generations);
    LoadBasicExtrasFromConfig();

    if (hLblSearchInfo_) {
        std::wstring info =
            L"類似一致は置換対象へ自動選択されません（内容を確認して手動で選択してください）。"
            L"指定行範囲・関数単位は完全一致検索と組み合わせて使えます。";
        SetEditText(hLblSearchInfo_, info);
    }
}

void App::SaveSearchOptionsToConfig()
{
    if (suppress_events_) return;
    if (!hChkSimilar_) return;
    cfg_.search_similar = IsChecked(hChkSimilar_);
    cfg_.search_similar_threshold = ReadSimilarThresholdFromUi();
    cfg_.search_line_range = IsChecked(hChkLineRange_);
    cfg_.search_line_from = ReadLineFromFromUi();
    cfg_.search_line_to = ReadLineToFromUi();
    cfg_.search_function = IsChecked(hChkFuncScope_);
    cfg_.search_function_name = util::Trim(GetEditText(hEditFuncName_));
    cfg_.report_markdown = IsChecked(hChkReportMd_);
    cfg_.report_json = IsChecked(hChkReportJson_);
    cfg_.winmerge_path = util::Trim(GetEditText(hEditWinMerge_));
    cfg_.undo_max_generations = ReadUndoMaxFromUi();
    cfg_.encoding_mode = ReadEncodingModeFromUi();
    undo_.SetMaxGenerations(cfg_.undo_max_generations);
}

std::vector<MatchResult> App::RunConfiguredSearch(const std::wstring& text,
                                                  const std::wstring& pattern,
                                                  const std::wstring& file_path,
                                                  std::wstring& out_error)
{
    out_error.clear();
    bool sim = IsChecked(hChkSimilar_);
    bool rng = IsChecked(hChkLineRange_);
    bool fnc = IsChecked(hChkFuncScope_);

    int f1 = 1, t1 = 0;
    if (fnc) {
        std::wstring fname = util::Trim(GetEditText(hEditFuncName_));
        if (fname.empty()) {
            out_error = L"関数名を入力してください（関数単位検索）。";
            return std::vector<MatchResult>();
        }
        int s = 0, e = 0;
        if (!CodeScope::FindFunctionBody(text, fname, s, e, out_error))
            return std::vector<MatchResult>();
        f1 = s; t1 = e;
    }

    int f2 = 1, t2 = 0;
    if (rng) {
        f2 = ReadLineFromFromUi();
        t2 = ReadLineToFromUi();
        if (t2 > 0 && t2 < f2) {
            out_error = L"指定行範囲が逆転しています（開始行 > 終了行）。";
            return std::vector<MatchResult>();
        }
    }

    int from = (f1 > f2) ? f1 : f2;
    int to;
    if      (t1 == 0) to = t2;
    else if (t2 == 0) to = t1;
    else              to = (t1 < t2) ? t1 : t2;
    if (to > 0 && from > to) return std::vector<MatchResult>();

    std::vector<MatchResult> r;
    if (sim) {
        r = SearchEngine::SearchSimilar(text, pattern, file_path, ReadSimilarThresholdFromUi());
        if (from > 1 || to > 0) r = SearchEngine::FilterByLineRange(r, from, to);
    } else {
        r = SearchEngine::Search(text, pattern, file_path);
        if (from > 1 || to > 0) r = SearchEngine::FilterByLineRange(r, from, to);
    }
    return r;
}

void App::ApplySearchOptionsToBatch(const std::wstring& pattern)
{
    if (!SearchOptionsEnabled()) return;
    if (IsChecked(hChkFuncScope_) && util::Trim(GetEditText(hEditFuncName_)).empty()) {
        log_.Warn(L"関数名が未入力のため、関数単位検索を行いませんでした。");
        return;
    }
    std::vector<MatchResult> out;
    for (size_t i = 0; i < batch_entries_.size(); ++i) {
        BatchFileEntry& e = batch_entries_[i];
        // 関数単位検索: 対象関数を持たないファイルはヒット 0 件として扱う
        if (IsChecked(hChkFuncScope_)) {
            int s = 0, en = 0;
            std::wstring ferr;
            if (!CodeScope::FindFunctionBody(e.text, util::Trim(GetEditText(hEditFuncName_)),
                                             s, en, ferr)) {
                e.hit_count = 0;
                e.status = L"対象外";
                continue;
            }
        }
        std::wstring err;
        std::vector<MatchResult> r = RunConfiguredSearch(e.text, pattern, e.path, err);
        if (!err.empty()) {
            log_.Warn(e.path + L": " + err);
            e.error = err;
            e.hit_count = 0;
            e.status = L"対象外";
            continue;
        }
        e.hit_count = (int)r.size();
        for (size_t k = 0; k < r.size(); ++k) out.push_back(r[k]);
    }
    matches_ = out;
}

void App::DoExportResults(int format)
{
    if (matches_.empty()) {
        ShowInfo(L"検索結果がありません。先に「検索」を実行してください。");
        return;
    }
    ExportFormat fmt = ExportFormat::Csv;
    if (format == 1)      fmt = ExportFormat::Markdown;
    else if (format == 2) fmt = ExportFormat::Json;

    std::wstring dir = ReportDirectory();
    if (!util::EnsureDirectory(dir)) {
        ShowError(L"出力先フォルダを作成できません: " + dir);
        return;
    }
    std::wstring base = IsFolderMode()
                        ? util::GetFileNameOf(batch_folder_)
                        : util::GetBaseNameNoExt(target_path_);
    if (base.empty()) base = L"result";

    std::wstring path = ResultExporter::BuildExportPath(dir, util::TimestampFileName(), fmt, base);
    std::wstring opid = util::NewOperationId();
    std::wstring err;
    if (!ResultExporter::Export(path, matches_, opid, fmt, cfg_.encoding_mode, err)) {
        ShowError(L"エクスポートに失敗しました。\n" + err);
        log_.Error(err);
        return;
    }
    cfg_.last_export_path = path;
    log_.Info(L"Exported search result: " + path);
    ShowInfo(std::wstring(L"検索結果を ") + ResultExporter::FormatName(fmt) +
             L" で出力しました。\n\n" + path);
    if (Confirm(L"出力したファイルを開きますか？\n\n" + path)) OpenPath(path);
}

std::wstring App::FindWinMergePath() const
{
    std::wstring p = util::Trim(GetEditText(hEditWinMerge_));
    if (p.empty()) p = cfg_.winmerge_path;
    if (!p.empty()) return util::PathExists(p) ? p : std::wstring();

    static const wchar_t* envs[3] = { L"ProgramFiles", L"ProgramFiles(x86)", L"LOCALAPPDATA" };
    static const wchar_t* rels[3] = {
        L"\\WinMerge\\WinMergeU.exe",
        L"\\WinMerge\\WinMerge.exe",
        L"\\Programs\\WinMerge\\WinMergeU.exe"
    };
    for (size_t e = 0; e < 3; ++e) {
        wchar_t buf[512];
        DWORD n = ::GetEnvironmentVariableW(envs[e], buf, 512);
        if (n == 0 || n >= 512) continue;
        for (size_t r = 0; r < 3; ++r) {
            std::wstring c = std::wstring(buf) + rels[r];
            if (util::PathExists(c)) return c;
        }
    }
    return std::wstring();
}

bool App::GetCompareItems(std::vector<CompareItem>& out, std::wstring& out_error)
{
    out.clear();
    out_error.clear();
    std::vector<MatchResult> sel = GetSelectedMatches();
    if (sel.empty()) {
        out_error = L"置換対象が選択されていません。検索結果タブで対象を選択してください。";
        return false;
    }

    OperationContext ctx;
    ReadInputs(ctx);

    // 選択された対象をファイル単位にまとめる（選択順を維持）
    std::vector<std::wstring> order;
    for (size_t i = 0; i < sel.size(); ++i) {
        bool found = false;
        for (size_t k = 0; k < order.size(); ++k)
            if (order[k] == sel[i].file_path) { found = true; break; }
        if (!found) order.push_back(sel[i].file_path);
    }

    for (size_t f = 0; f < order.size(); ++f) {
        CompareItem item;
        item.path = order[f];

        std::vector<MatchResult> mine;
        for (size_t i = 0; i < sel.size(); ++i)
            if (sel[i].file_path == order[f]) mine.push_back(sel[i]);

        std::wstring text;
        if (IsFolderMode()) {
            for (size_t i = 0; i < batch_entries_.size(); ++i) {
                if (batch_entries_[i].path == item.path) { text = batch_entries_[i].text; break; }
            }
        } else {
            item.path = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
            text = file_text_;
        }
        if (text.empty()) {
            FileInfo fi;
            if (!FileManager::ReadText(item.path, cfg_.encoding_mode, text, fi)) {
                out_error = L"比較対象のファイルを読み取れません: " + item.path;
                return false;
            }
        }

        std::vector<std::wstring> blocks = MakeBlocksFor(mine, ctx, text);
        std::wstring after = PreviewGenerator::BuildAfterTextEach(text, mine, blocks);
        ApplyLogInsertion(after, mine);
        if (ctx.add_switch_header_include && !ctx.switch_header_path.empty()) {
            bool added = false;
            int line = 0;
            ApplyIncludeToText(after, item.path, ctx, added, line);
        }
        item.before = text;
        item.after = after;
        out.push_back(item);
    }
    return true;
}

void App::DoWinMerge()
{
    std::vector<CompareItem> items;
    std::wstring err;
    if (!GetCompareItems(items, err)) {
        ShowError(err.empty() ? std::wstring(L"比較する内容を作成できません。") : err);
        return;
    }

    std::wstring exe = FindWinMergePath();
    if (exe.empty()) {
        ShowError(L"WinMerge が見つかりません。\n\n"
                  L"「検索条件」タブの「WinMergeU.exe のパス」で指定してください。\n"
                  L"既定の確認先: C:\\Program Files\\WinMerge\\WinMergeU.exe");
        log_.Warn(L"WinMerge not found.");
        return;
    }

    // 選択されたファイルが多数ある場合の上限（1 ファイルにつき 1 ウィンドウ開くため）
    const size_t kMaxFiles = 10;
    size_t count = items.size();
    if (count > kMaxFiles) count = kMaxFiles;

    int launched = 0;
    for (size_t n = 0; n < count; ++n) {
        const CompareItem& item = items[n];
        std::wstring ext = util::GetExtensionOf(item.path);
        if (ext.empty()) ext = L".txt";
        std::wstring pre = L"ms_before_" + std::to_wstring((int)(n + 1));
        std::wstring post = L"ms_after_" + std::to_wstring((int)(n + 1));
        std::wstring t1 = FileManager::MakeTempFilePath(pre.c_str(), ext);
        std::wstring t2 = FileManager::MakeTempFilePath(post.c_str(), ext);
        std::wstring werr;
        if (!FileManager::WriteTextAtomic(t1, item.before, TextEncoding::UTF8, true, NewlineKind::CRLF, werr) ||
            !FileManager::WriteTextAtomic(t2, item.after,  TextEncoding::UTF8, true, NewlineKind::CRLF, werr)) {
            ShowError(L"比較用の一時ファイルを作成できません。\n" + werr);
            return;
        }

        std::wstring fname = util::GetFileNameOf(item.path);
        std::wstring args = L"\"" + t1 + L"\" \"" + t2 + L"\" /e /u";
        // 複数ファイルを同時に比較するときは別ウィンドウで開く（/s- = 単一インスタンス設定を無視）
        if (items.size() > 1) args += L" /s-";
        args += L" /dl \"変換前: " + fname + L"\" /dr \"変換後: " + fname + L"\"";

        HINSTANCE r = ::ShellExecuteW(nullptr, L"open", exe.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)r <= 32) {
            ShowError(L"WinMerge を起動できませんでした。\n\n" + exe);
            log_.Error(L"Failed to launch WinMerge: " + exe);
            return;
        }
        ++launched;
        log_.Info(L"WinMerge launched: " + exe);
        log_.Info(L"  " + fname);
        log_.Info(L"    変換前: " + t1);
        log_.Info(L"    変換後: " + t2);
    }

    std::wstring msg = L"WinMerge で比較しています（変換前 / 変換後）: "
                     + std::to_wstring(launched) + L" ファイル。";
    if (items.size() > kMaxFiles)
        msg += L"（選択が " + std::to_wstring((int)items.size()) + L" 件のため先頭 "
               + std::to_wstring((int)kMaxFiles) + L" 件のみ比較しました）";
    SetStatus(msg);
}

void App::DoWinMergeBrowse()
{
    wchar_t buf[MAX_PATH * 4];
    ::ZeroMemory(buf, sizeof(buf));
    std::wstring cur = util::Trim(GetEditText(hEditWinMerge_));
    lstrcpynW(buf, cur.c_str(), MAX_PATH * 4);

    OPENFILENAMEW ofn;
    ::ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd_;
    ofn.lpstrFilter = L"WinMerge (WinMergeU.exe)\0WinMergeU.exe\0実行ファイル (*.exe)\0*.exe\0すべて (*.*)\0*.*\0\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrTitle = L"WinMergeU.exe を選択してください";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!::GetOpenFileNameW(&ofn)) return;

    SetEditText(hEditWinMerge_, buf);
    SaveSearchOptionsToConfig();
    log_.Info(L"WinMerge path set: " + std::wstring(buf));
}

void App::WriteExtraReportsAfterRun(bool batch_mode)
{
    // 追加機能: 変更単位以外（ファイル / プロジェクト単位）のレポートを先に出す
    WriteScopeReports(batch_mode);

    if (!cfg_.report_markdown && !cfg_.report_json) return;

    OperationContext ctx;
    ReadInputs(ctx);
    std::wstring stamp = util::TimestampFileName();
    std::wstring dir = ReportDirectory();
    if (!util::EnsureDirectory(dir)) {
        log_.Warn(L"レポート出力先を作成できません: " + dir);
        return;
    }

    if (!batch_mode) {
        if (last_result_.status.empty()) return;
        if (target_path_.empty()) return;
        ctx.operation_id = last_result_.operation_id;
        ctx.target_file_path = last_result_.target_file_path;
        std::wstring err;
        if (cfg_.report_markdown) {
            std::wstring p = DiffReportGenerator::BuildReportPathExt(dir, target_path_, stamp, L".md");
            if (DiffReportGenerator::GenerateMarkdown(p, ctx, matches_, last_result_.before_text,
                                                      last_result_.after_text, last_result_, err))
                log_.Info(L"Markdown レポートを出力しました: " + p);
            else
                log_.Warn(L"Markdown レポートの出力に失敗しました: " + err);
        }
        if (cfg_.report_json) {
            std::wstring p = DiffReportGenerator::BuildReportPathExt(dir, target_path_, stamp, L".json");
            if (DiffReportGenerator::GenerateJson(p, ctx, matches_, last_result_.before_text,
                                                  last_result_.after_text, last_result_, err))
                log_.Info(L"JSON レポートを出力しました: " + p);
            else
                log_.Warn(L"JSON レポートの出力に失敗しました: " + err);
        }
        return;
    }

    std::vector<ReportFileSection> sections;
    for (size_t i = 0; i < batch_entries_.size(); ++i) {
        BatchFileEntry& e = batch_entries_[i];
        std::vector<MatchResult> sel;
        for (size_t k = 0; k < matches_.size(); ++k)
            if (matches_[k].selected && matches_[k].file_path == e.path) sel.push_back(matches_[k]);
        if (sel.empty()) continue;

        ReportFileSection sec;
        sec.file_path = e.path;
        sec.matches = sel;
        sec.before_text = e.text;
        std::vector<std::wstring> blocks = MakeBlocksFor(sel, ctx, e.text);
        sec.after_text = PreviewGenerator::BuildAfterTextEach(e.text, sel, blocks);
        ApplyLogInsertion(sec.after_text, sel);
        sec.encoding_name = EncodingDetector::EncodingName(e.info.encoding);
        sec.newline_name = util::NewlineString(e.info.newline);
        sec.include_added = e.include_added;
        sec.include_line = e.include_line;
        sec.result.file_path = e.path;
        sec.result.status = e.status;
        sec.result.error = e.error;
        sections.push_back(sec);
    }
    if (sections.empty()) return;
    ctx.operation_id = last_batch_result_.operation_id;
    ctx.target_folder = batch_folder_;

    std::wstring err;
    if (cfg_.report_markdown) {
        std::wstring p = DiffReportGenerator::BuildReportPathExt(dir, batch_folder_, stamp, L".md");
        if (DiffReportGenerator::GenerateIntegratedMarkdown(p, ctx, sections, last_batch_result_, err))
            log_.Info(L"Markdown 統合レポートを出力しました: " + p);
        else
            log_.Warn(L"Markdown 統合レポートの出力に失敗しました: " + err);
    }
    if (cfg_.report_json) {
        std::wstring p = DiffReportGenerator::BuildReportPathExt(dir, batch_folder_, stamp, L".json");
        if (DiffReportGenerator::GenerateIntegratedJson(p, ctx, sections, last_batch_result_, err))
            log_.Info(L"JSON 統合レポートを出力しました: " + p);
        else
            log_.Warn(L"JSON 統合レポートの出力に失敗しました: " + err);
    }
}

void App::DoAiTest()
{
    AiSettings st = ReadAiSettings();
    if (st.api_key.empty()) {
        ShowError(L"API キーが未設定です。「基本設定」タブの「AI 連携」で入力してください。");
        return;
    }
    SetStatus(L"AI へ接続テスト中...");
    std::wstring text, err;
    bool ok = AiClient::Ask(st, L"接続テストです。「OK」とだけ返してください。",
                            text, err, std::function<void(const std::wstring&)>(),
                            [this]() { PumpMessages(); });
    if (ok) {
        log_.Info(L"AI 接続テスト: 成功");
        ShowInfo(L"AI への接続に成功しました。\n\n応答: " + util::Trim(text));
    } else {
        log_.Error(L"AI 接続テスト: 失敗 - " + err);
        ShowError(L"AI への接続に失敗しました。\n\n" + err);
    }
    SetStatus(ok ? L"AI 接続テスト: 成功" : L"AI 接続テスト: 失敗");
}

bool App::GenerateSpecs(bool do_change, bool do_inspect)
{
    if (generating_) return false;

    AiSettings st = ReadAiSettings();
    if (st.api_key.empty()) {
        ShowError(L"API キーが未設定です。「基本設定」タブの「AI 連携」で API キーを入力してください。");
        log_.Warn(L"AI 連携: API キーが未設定です。");
        return false;
    }
    SpecInput in = BuildSpecInput();
    if (in.system_name.empty()) {
        ShowError(L"「システム情報」タブの「システム名」を入力してください。");
        return false;
    }

    int kinds[2];
    int n = 0;
    if (do_change)  kinds[n++] = SPEC_CHANGE;
    if (do_inspect) kinds[n++] = SPEC_INSPECTION;
    if (n == 0) return false;

    generating_ = true;
    SetState(AppState::EXECUTING);
    SetProgress(10);
    log_.Info(L"AI 連携: 仕様書生成を開始しました（" + st.endpoint + L"）");

    std::wstring md_all, err, last_path;
    bool any_ok = false;

    for (int k = 0; k < n; ++k) {
        int kind = kinds[k];
        std::wstring label = SpecGenerator::KindLabel(kind);
        std::wstring prompt = (kind == SPEC_INSPECTION)
                              ? SpecGenerator::BuildInspectionSpecPrompt(in)
                              : SpecGenerator::BuildChangeSpecPrompt(in);
        SetStatus(L"AI が" + label + L"を生成しています...");
        log_.Info(L"AI 連携: " + label + L" を要求しました。");

        std::wstring text;
        bool ok = AiClient::Ask(st, prompt, text, err,
                                [this, label](const std::wstring&) {
                                    SetStatus(L"AI が" + label + L"を生成中...");
                                },
                                [this]() { PumpMessages(); });
        if (!ok) {
            log_.Error(L"AI 連携: " + label + L" の生成に失敗しました: " + err);
            ShowError(L"AI 連携でエラーが発生しました。\n\n" + err);
            continue;
        }
        any_ok = true;
        log_.Info(L"AI 連携: " + label + L" を受信しました（" +
                  std::to_wstring((int)text.size()) + L" 文字）。");

        if (cfg_.create_html_report) {
            std::wstring dir = ReportDirectory();
            util::EnsureDirectory(dir);
            std::wstring path = util::JoinPath(
                dir, SpecGenerator::MakeReportFileName(in.system_name, kind, util::NowDateTime()));
            std::wstring html = SpecGenerator::WrapReportHtml(
                label + L" - " + in.system_name, SpecGenerator::MarkdownToHtml(text), in);
            std::wstring werr;
            if (SpecGenerator::WriteReportFile(path, html, werr)) {
                last_path = path;
                log_.Info(L"AI 連携: レポートを出力しました: " + path);
            } else {
                log_.Warn(L"AI 連携: " + werr);
            }
        }

        if (!md_all.empty()) md_all += L"\r\n\r\n";
        md_all += L"===== " + label + L" =====\r\n\r\n" + util::NormalizeNewlines(text);
    }

    SetEditText(hEditSpecOut_, md_all);
    if (!last_path.empty()) {
        last_spec_report_path_ = last_path;
        cfg_.last_spec_report_path = last_path;
        SetEditText(hLblSpecInfo_, L"出力: " + last_path);
    }
    SetProgress(100);
    generating_ = false;
    SetState(AppState::READY);
    UpdateButtons();

    if (any_ok) {
        SetStatus(L"仕様書の生成が完了しました。");
        if (!last_path.empty() && (IsChecked(hChkAiOpenReport_) || cfg_.ai_open_report))
            OpenPath(last_path);
        ShowInfo(L"仕様書を生成しました。" +
                 (last_path.empty() ? L"" : L"\n\n" + last_path));
    }
    return any_ok;
}


// -----------------------------------------------------------------------------
// AI を使わない運用: 依頼文を txt で出力する / 貼り付けた回答をレポート化する
// -----------------------------------------------------------------------------
void App::DoExportPrompt(int kind)
{
    ExportPrompts(kind == SPEC_CHANGE, kind == SPEC_INSPECTION);
}

void App::DoExportPromptBoth()
{
    ExportPrompts(true, true);
}

bool App::ExportPrompts(bool do_change, bool do_inspect)
{
    SpecInput in = BuildSpecInput();
    if (in.system_name.empty()) {
        ShowError(L"「システム情報」タブの「システム名」を入力してください。\n"
                  L"（ファイル名に使用します）");
        return false;
    }

    int kinds[2];
    int n = 0;
    if (do_change)  kinds[n++] = SPEC_CHANGE;
    if (do_inspect) kinds[n++] = SPEC_INSPECTION;
    if (n == 0) return false;

    std::wstring dir = ReportDirectory();
    util::EnsureDirectory(dir);

    std::wstring paths;
    std::wstring first;
    for (int k = 0; k < n; ++k) {
        int kind = kinds[k];
        std::wstring txt = SpecGenerator::BuildPromptTxt(kind, in);
        std::wstring path = util::JoinPath(
            dir, SpecGenerator::MakePromptFileName(in.system_name, kind, util::NowDateTime()));
        std::wstring werr;
        if (!SpecGenerator::WriteReportFile(path, txt, werr)) {
            log_.Warn(L"プロンプト出力: " + werr);
            ShowError(werr);
            continue;
        }
        if (first.empty()) first = path;
        if (!paths.empty()) paths += L"\n";
        paths += path;
        log_.Info(L"プロンプトを出力しました: " + path);
    }
    if (first.empty()) return false;

    SetEditText(hLblSpecInfo_, L"プロンプト出力: " + first);
    SetStatus(L"プロンプト txt を出力しました。AI チャットへ貼り付けてください。");
    ShowInfo(L"AI チャットへ貼り付ける依頼文を出力しました。\n\n" + paths +
             L"\n\n【手順】\n1. 上記 txt を開き、内容をすべてコピー\n"
             L"2. AI チャット（ChatGPT / Claude / Gemini など）へ貼り付けて送信\n"
             L"3. 返ってきた回答をコピーし、このタブの出力欄へ貼り付けて\n"
             L"   「この内容を変更仕様書にする / 検査仕様書にする」を押す\n"
             L"   → HTML レポートが作成されます");

    if (IsChecked(hChkAiOpenReport_) || cfg_.ai_open_report) OpenPath(first);
    return true;
}

void App::DoMakeReportFromText(int kind)
{
    std::wstring text = GetEditText(hEditSpecOut_);
    if (util::Trim(text).empty()) {
        ShowError(L"出力欄が空です。\n\nAI チャットの回答をコピーして、この欄に貼り付けてください。\n"
                  L"（「プロンプトtxt」ボタンで依頼文を出力できます）");
        return;
    }
    SpecInput in = BuildSpecInput();
    if (in.system_name.empty()) in.system_name = L"system";
    in.created_at = util::NowDateTime();

    std::wstring label = SpecGenerator::KindLabel(kind);
    if (!cfg_.create_html_report) {
        ShowWarn(L"「HTML差分レポートを出力する」が無効です。\n"
                 L"「基本設定」タブで有効にするとレポートを作成できます。");
        return;
    }

    std::wstring dir = ReportDirectory();
    util::EnsureDirectory(dir);
    std::wstring path = util::JoinPath(
        dir, SpecGenerator::MakeReportFileName(in.system_name, kind, util::NowDateTime()));
    std::wstring html = SpecGenerator::WrapReportHtml(
        label + L" - " + in.system_name, SpecGenerator::MarkdownToHtml(text), in);
    std::wstring werr;
    if (!SpecGenerator::WriteReportFile(path, html, werr)) {
        log_.Error(L"貼り付け内容のレポート化に失敗: " + werr);
        ShowError(werr);
        return;
    }
    last_spec_report_path_ = path;
    cfg_.last_spec_report_path = path;
    SetEditText(hLblSpecInfo_, L"出力: " + path);
    log_.Info(L"貼り付けた回答から" + label + L"を作成しました: " + path);
    SetStatus(label + L"を作成しました。");
    ShowInfo(label + L"を作成しました。\n\n" + path);
    if (IsChecked(hChkAiOpenReport_) || cfg_.ai_open_report) OpenPath(path);
}


// =============================================================================
// 追加機能: ログ出力 / レポート作成単位 / GoogleTest
// =============================================================================
ReportScope App::ReadReportScopeFromUi() const
{
    int sel = (int)::SendMessageW(hCmbReportScope_, CB_GETCURSEL, 0, 0);
    if (sel == 1) return ReportScope::File;
    if (sel == 2) return ReportScope::Project;
    return ReportScope::Change;
}

void App::LoadBasicExtrasFromConfig()
{
    if (!hChkLogEnabled_) return;
    suppress_events_ = true;
    SetChecked(hChkLogEnabled_, cfg_.log_enabled);
    SetChecked(hChkLogTemp_, cfg_.log_insert_temp);
    SetChecked(hChkLogRet_, cfg_.log_return_value);
    int sel = 0;
    if (cfg_.report_scope == ReportScope::File)         sel = 1;
    else if (cfg_.report_scope == ReportScope::Project) sel = 2;
    ::SendMessageW(hCmbReportScope_, CB_SETCURSEL, sel, 0);
    SetChecked(hChkGTest_, cfg_.gtest_enabled);
    SetEditText(hEditLoadIter_, std::to_wstring(cfg_.gtest_load_iterations));
    suppress_events_ = false;
}

void App::SaveBasicExtrasToConfig()
{
    if (suppress_events_) return;
    if (!hChkLogEnabled_) return;
    cfg_.log_enabled      = IsChecked(hChkLogEnabled_);
    cfg_.log_insert_temp  = IsChecked(hChkLogTemp_);
    cfg_.log_return_value = IsChecked(hChkLogRet_);
    cfg_.report_scope     = ReadReportScopeFromUi();
    cfg_.gtest_enabled    = IsChecked(hChkGTest_);
    int v = ParseIntOr(util::Trim(GetEditText(hEditLoadIter_)), 1000);
    if (v < 0) v = 0;
    cfg_.gtest_load_iterations = v;
    cfgmgr_.SaveExtras(cfg_);
}

void App::ApplyLogInsertion(std::wstring& text, const std::vector<MatchResult>& sel)
{
    if (!cfg_.log_enabled || sel.empty()) return;

    LogInsertOptions o;
    o.header_name = cfg_.log_header_name.empty()
                  ? std::wstring(L"ms_migration_log.h") : cfg_.log_header_name;
    o.switch_name = util::Trim(GetEditText(hSwitchEdit_));
    o.insert_input = true;
    o.insert_output = true;
    o.insert_temp = cfg_.log_insert_temp;
    o.log_return_value = cfg_.log_return_value;

    LogInsertResult r;
    std::wstring err;
    if (!CodeLogger::Apply(text, sel, o, r, err)) {
        log_.Warn(L"ログ挿入に失敗しました: " + err);
        return;
    }
    log_.Info(L"ログ挿入: 関数 " + std::to_wstring(r.functions) +
              L" / INPUT " + std::to_wstring(r.input_logs) +
              L" / OUTPUT " + std::to_wstring(r.output_logs) +
              L" / TEMP " + std::to_wstring(r.temp_logs));
    for (size_t i = 0; i < r.warnings.size(); ++i) log_.Warn(r.warnings[i]);
}

void App::DoWriteLogHeader()
{
    std::wstring dir;
    if (IsFolderMode()) {
        dir = util::Trim(GetEditText(hFolderEdit_));
    } else {
        std::wstring t = util::Trim(GetEditText(hTargetEdit_));
        if (!t.empty()) dir = util::GetDirectoryOf(util::MakeAbsolute(t));
    }
    if (dir.empty()) {
        ShowError(L"出力先を決められません。対象ファイルまたは対象フォルダを指定してください。");
        return;
    }

    SaveExtraUi();          // 画面の設定（拡張子・ON/OFF）を確定させる
    LogRuntimeOptions o;
    if (!cfg_.log_header_name.empty()) o.header_name = cfg_.log_header_name;
    o.default_enabled = cfg_.log_enabled;
    o.source_is_cpp   = cfg_.log_source_cpp;
    o.source_name     = cfg_.log_source_cpp ? L"ms_migration_log.cpp" : L"ms_migration_log.c";

    std::vector<std::wstring> files;
    std::wstring err;
    if (!LogRuntimeGenerator::WriteTo(dir, o, files, err)) {
        ShowError(err);
        log_.Error(err);
        return;
    }
    std::wstring msg = L"次のファイルを出力しました。\n\n";
    for (size_t i = 0; i < files.size(); ++i) {
        msg += files[i] + L"\n";
        log_.Info(L"ログ定義を出力しました: " + files[i]);
    }
    msg += L"\n" + LogRuntimeGenerator::UsageText();
    ShowInfo(msg);
}

// =============================================================================
// GoogleTest の対象範囲（実行履歴の差分から対象関数を決める）
// =============================================================================
static void ChangedLinesOfAfter(const std::wstring& before, const std::wstring& after,
                                std::vector<int>& out)
{
    std::vector<std::wstring> lb, la;
    std::wstring cur;
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i] == L'\n') { lb.push_back(cur); cur.clear(); }
        else if (before[i] != L'\r') cur.push_back(before[i]);
    }
    lb.push_back(cur);
    cur.clear();
    for (size_t i = 0; i < after.size(); ++i) {
        if (after[i] == L'\n') { la.push_back(cur); cur.clear(); }
        else if (after[i] != L'\r') cur.push_back(after[i]);
    }
    la.push_back(cur);

    const size_t n = lb.size(), m = la.size();
    if (n > 6000 || m > 6000) {   // 大きすぎるファイルは全行を対象にする
        for (size_t j = 0; j < m; ++j) out.push_back((int)j);
        return;
    }

    std::vector<std::vector<int> > dp(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (lb[i - 1] == la[j - 1]) dp[i][j] = dp[i - 1][j - 1] + 1;
            else dp[i][j] = (dp[i - 1][j] >= dp[i][j - 1]) ? dp[i - 1][j] : dp[i][j - 1];
        }
    }

    size_t i = n, j = m;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && lb[i - 1] == la[j - 1]) { --i; --j; }
        else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            out.push_back((int)(j - 1));   // 変更後の行
            --j;
        } else if (i > 0) {
            --i;                           // 削除行は対象外
        }
    }
}

bool App::CollectRangeGTestTargets(std::vector<GTestTarget>& out, std::wstring& out_error)
{
    out.clear();
    out_error.clear();

    std::wstring fid, tid;
    if (!GetPairIds(hCmbGtRangeFrom_, hCmbGtRangeTo_, fid, tid)) {
        // 「指定なし」のいずれか → 範囲指定なし（呼び出し側で従来処理）
        return false;
    }

    std::wstring base = HistoryBaseDir();
    std::vector<HistoryFileEntry> rf = HistoryManager::ListFiles(base, fid);
    std::vector<HistoryFileEntry> rt = HistoryManager::ListFiles(base, tid);
    if (rf.empty() || rt.empty()) {
        out_error = L"指定した範囲のスナップショットがありません。";
        return true;
    }

    for (size_t a = 0; a < rt.size(); ++a) {
        const HistoryFileEntry* b = nullptr;
        for (size_t k = 0; k < rf.size(); ++k)
            if (rf[k].path == rt[a].path) { b = &rf[k]; break; }
        if (!b || !b->has_before || !rt[a].has_after) continue;

        std::wstring tb, ta, err;
        if (!HistoryManager::LoadText(b->before, tb, err)) continue;
        if (!HistoryManager::LoadText(rt[a].after, ta, err)) continue;
        if (tb == ta) continue;

        std::vector<int> changed;
        ChangedLinesOfAfter(tb, ta, changed);
        if (changed.empty()) continue;

        // 変更行の先頭オフセットから、その行を含む関数を対象にする
        std::vector<size_t> line_start;
        line_start.push_back(0);
        for (size_t i = 0; i < ta.size(); ++i)
            if (ta[i] == L'\n') line_start.push_back(i + 1);

        std::vector<MatchResult> ms;
        for (size_t c = 0; c < changed.size(); ++c) {
            size_t li = (size_t)changed[c];
            if (li >= line_start.size()) continue;
            MatchResult m;
            m.match_id = (int)ms.size();
            m.file_path = rt[a].path;
            m.start_offset = line_start[li];
            size_t le = ta.find(L'\n', m.start_offset);
            m.end_offset = (le == std::wstring::npos) ? ta.size() : le;
            m.start_line = (int)li + 1;
            m.selected = true;
            ms.push_back(m);
        }
        if (ms.empty()) continue;

        std::vector<GTestTarget> t = GTestGenerator::CollectTargets(ta, rt[a].path, ms);
        out.insert(out.end(), t.begin(), t.end());
    }

    // 関数の重複を除去
    std::vector<GTestTarget> uniq;
    for (size_t i = 0; i < out.size(); ++i) {
        bool dup = false;
        for (size_t k = 0; k < uniq.size(); ++k)
            if (uniq[k].file_path == out[i].file_path && uniq[k].func_name == out[i].func_name)
                { dup = true; break; }
        if (!dup) uniq.push_back(out[i]);
    }
    out.swap(uniq);
    if (out.empty()) {
        out_error = L"指定した範囲に、テスト対象となる関数の変更がありません。";
    }
    return true;
}

void App::DoGenerateGTest()
{
    std::vector<GTestTarget> targets;
    std::wstring range_err;
    bool range_used = CollectRangeGTestTargets(targets, range_err);

    if (range_used) {
        if (targets.empty()) {
            ShowError(range_err.empty() ? L"テスト対象の関数がありません。" : range_err);
            return;
        }
    } else if (IsFolderMode()) {
        for (size_t i = 0; i < batch_entries_.size(); ++i) {
            std::vector<MatchResult> sel;
            for (size_t k = 0; k < matches_.size(); ++k)
                if (matches_[k].selected && matches_[k].file_path == batch_entries_[i].path)
                    sel.push_back(matches_[k]);
            if (sel.empty()) continue;
            std::vector<GTestTarget> t = GTestGenerator::CollectTargets(
                batch_entries_[i].text, batch_entries_[i].path, sel);
            targets.insert(targets.end(), t.begin(), t.end());
        }
    } else {
        std::vector<MatchResult> sel = GetSelectedMatches();
        if (sel.empty()) {
            ShowError(L"置換対象が選択されていません。検索結果タブで対象を選択してください。\n"
                      L"（履歴タブの範囲を指定して、過去の変更分から生成することもできます）");
            return;
        }
        targets = GTestGenerator::CollectTargets(file_text_, target_path_, sel);
    }

    if (targets.empty()) {
        ShowError(L"テスト対象の関数がありません（関数を判定できない一致のみです）。");
        return;
    }

    std::wstring root = IsFolderMode()
        ? util::Trim(GetEditText(hFolderEdit_))
        : util::GetDirectoryOf(target_path_);
    if (root.empty()) root = HistoryBaseDir();

    GTestOptions o;
    o.out_dir = cfg_.gtest_dir.empty() ? GTestGenerator::DefaultOutDir(root) : cfg_.gtest_dir;
    o.load_iterations = cfg_.gtest_load_iterations;
    o.generate_load   = cfg_.gtest_load_enabled;
    o.switch_name     = util::Trim(GetEditText(hSwitchEdit_));

    std::vector<std::wstring> files;
    std::wstring err;
    if (!GTestGenerator::WriteProject(root, targets, o, files, err)) {
        ShowError(err);
        log_.Error(err);
        return;
    }
    std::wstring msg = L"GoogleTest を生成しました（" + std::to_wstring((int)targets.size()) +
                       L" 関数）。\n";
    if (range_used) {
        std::wstring fid, tid;
        GetPairIds(hCmbGtRangeFrom_, hCmbGtRangeTo_, fid, tid);
        msg += L"対象範囲: " + fid + L" ～ " + tid + L" の変更箇所\n";
    }
    msg += L"\n";
    for (size_t i = 0; i < files.size(); ++i) msg += files[i] + L"\n";
    msg += L"\n「テスト実行」ボタンでビルドと実行を行えます。";
    ShowInfo(msg);
    log_.Info(L"GoogleTest generated: " + std::to_wstring((int)targets.size()) + L" functions");
}


void App::DoRunGTest()
{
    std::wstring root = IsFolderMode()
        ? util::Trim(GetEditText(hFolderEdit_))
        : util::GetDirectoryOf(target_path_);
    if (root.empty()) {
        ShowError(L"対象フォルダ / 対象ファイルを指定してください。");
        return;
    }
    GTestOptions o;
    o.out_dir = cfg_.gtest_dir.empty() ? GTestGenerator::DefaultOutDir(root) : cfg_.gtest_dir;
    o.load_iterations = cfg_.gtest_load_iterations;
    if (!util::PathExists(util::JoinPath(o.out_dir, L"CMakeLists.txt"))) {
        ShowError(L"テストプロジェクトがありません。先に「テスト生成」を実行してください。");
        return;
    }

    SetStatus(L"GoogleTest をビルドしています...");
    log_.Info(L"GoogleTest: build & run 開始 (" + o.out_dir + L")");

    GTestRunResult r;
    std::wstring err;
    if (!GTestGenerator::BuildAndRun(root, o, r, err)) {
        ShowError(err);
        log_.Error(err);
        SetStatus(L"GoogleTest のビルドに失敗しました。");
        return;
    }

    std::wstring msg = L"GoogleTest を実行しました。\n\n" + r.summary + L"\n\n";
    msg += L"実行ファイル: " + r.exe_path + L"\n";
    msg += L"結果 JSON: " + r.json_path + L"\n";
    msg += L"実行ログ: " + r.run_log + L"\n";
    ShowInfo(msg);
    log_.Info(L"GoogleTest: " + r.summary);
    SetStatus(L"GoogleTest: " + r.summary);
}

void App::WriteScopeReports(bool batch_mode)
{
    if (cfg_.report_scope == ReportScope::Change) return;

    std::vector<ScopeEntry> entries;
    OperationContext ctx;
    ReadInputs(ctx);

    if (!batch_mode) {
        if (target_path_.empty() || last_result_.status.empty()) return;
        ScopeEntry e;
        e.file_path = target_path_;
        e.before_text = last_result_.before_text;
        e.after_text = last_result_.after_text;
        e.encoding_name = EncodingDetector::EncodingName(file_info_.encoding);
        e.newline_name = util::NewlineString(file_info_.newline);
        e.status = last_result_.status;
        for (size_t i = 0; i < matches_.size(); ++i)
            if (matches_[i].selected) e.matches.push_back(matches_[i]);
        if (e.matches.empty()) return;
        entries.push_back(e);
    } else {
        for (size_t i = 0; i < batch_entries_.size(); ++i) {
            BatchFileEntry& b = batch_entries_[i];
            ScopeEntry e;
            e.file_path = b.path;
            e.before_text = b.text;
            e.encoding_name = EncodingDetector::EncodingName(b.info.encoding);
            e.newline_name = util::NewlineString(b.info.newline);
            e.status = b.status;
            e.error = b.error;
            for (size_t k = 0; k < matches_.size(); ++k)
                if (matches_[k].selected && matches_[k].file_path == b.path)
                    e.matches.push_back(matches_[k]);
            if (e.matches.empty()) continue;
            std::vector<std::wstring> blocks = MakeBlocksFor(e.matches, ctx, b.text);
            e.after_text = PreviewGenerator::BuildAfterTextEach(b.text, e.matches, blocks);
            ApplyLogInsertion(e.after_text, e.matches);
            entries.push_back(e);
        }
        if (entries.empty()) return;
    }

    std::wstring dir = ReportDirectory();
    if (!util::EnsureDirectory(dir)) {
        log_.Warn(L"レポート出力先を作成できません: " + dir);
        return;
    }
    std::wstring stamp = util::TimestampFileName();
    std::wstring base;
    if (IsFolderMode()) base = util::GetFileNameOf(util::Trim(GetEditText(hFolderEdit_)));
    else                base = util::GetFileNameOf(target_path_);
    if (base.empty()) base = L"migration";

    std::wstring sysname = util::Trim(GetEditText(hEditSysName_));
    std::wstring md = ScopeReportGenerator::BuildMarkdown(
        sysname, ctx.switch_name, util::NowDateTime(), cfg_.report_scope, entries);

    std::wstring err;
    std::wstring p = util::JoinPath(dir, ScopeReportGenerator::MakeFileName(
        base, cfg_.report_scope, stamp, L".md"));
    if (ScopeReportGenerator::Write(p, md, err))
        log_.Info(L"スコープレポート (Markdown) を出力しました: " + p);
    else
        log_.Warn(L"スコープレポートの出力に失敗しました: " + err);

    std::wstring html = ScopeReportGenerator::BuildHtmlFromEntries(
        base, ctx.switch_name, util::NowDateTime(), cfg_.report_scope, entries);
    std::wstring ph = util::JoinPath(dir, ScopeReportGenerator::MakeFileName(
        base, cfg_.report_scope, stamp, L".html"));
    if (ScopeReportGenerator::Write(ph, html, err))
        log_.Info(L"スコープレポート (HTML) を出力しました: " + ph);
    else
        log_.Warn(L"スコープレポート (HTML) の出力に失敗しました: " + err);

    if (IsChecked(hChkOpenReport_) && util::PathExists(ph)) OpenPath(ph);
}


// =============================================================================
// 履歴（undobak） / 比較範囲 / 追加コード
// =============================================================================
static bool FilesEqual(const std::wstring& a, const std::wstring& b)
{
    std::ifstream fa(a.c_str(), std::ios::binary);
    std::ifstream fb(b.c_str(), std::ios::binary);
    if (!fa || !fb) return false;
    char ba[8192], bb[8192];
    while (true) {
        fa.read(ba, sizeof(ba));
        fb.read(bb, sizeof(bb));
        std::streamsize na = fa.gcount(), nb = fb.gcount();
        if (na != nb) return false;
        if (na == 0) return true;
        if (memcmp(ba, bb, (size_t)na) != 0) return false;
    }
}

static std::wstring TempSnapshotPath(const std::wstring& tag, int idx)
{
    wchar_t dir[MAX_PATH];
    DWORD n = ::GetTempPathW(MAX_PATH, dir);
    std::wstring base = (n > 0) ? std::wstring(dir) : std::wstring(L".\\");
    wchar_t buf[64];
    swprintf(buf, 64, L"ms_snap_%d_%03d_%d.tmp", (int)::GetCurrentProcessId(), idx,
             (int)::GetTickCount64());
    return util::JoinPath(base, buf);
}

std::wstring App::HistoryBaseDir() const
{
    if (IsFolderMode()) {
        std::wstring f = util::Trim(GetEditText(hFolderEdit_));
        if (!f.empty()) return util::MakeAbsolute(f);
    }
    if (!target_path_.empty()) return util::GetDirectoryOf(target_path_);
    std::wstring b = BackupDirectory();
    return b.empty() ? util::ModuleDirectory() : b;
}

void App::LoadExtraUi()
{
    if (!hChkLogEnabled_) return;
    SetChecked(hChkLogEnabled_, cfg_.log_enabled);
    SetChecked(hChkLogTemp_, cfg_.log_insert_temp);
    SetChecked(hChkLogRet_, cfg_.log_return_value);
    SetChecked(hChkGTest_, cfg_.gtest_enabled);
    SetChecked(hChkLoadTest_, cfg_.gtest_load_enabled);
    SetEditText(hEditLoadIter_, std::to_wstring(cfg_.gtest_load_iterations));
    int rs = 0;
    if (cfg_.report_scope == ReportScope::File) rs = 1;
    else if (cfg_.report_scope == ReportScope::Project) rs = 2;
    ::SendMessageW(hCmbReportScope_, CB_SETCURSEL, rs, 0);
    ::SendMessageW(hCmbLogExt_, CB_SETCURSEL, cfg_.log_source_cpp ? 1 : 0, 0);
    if (hEditLoadIter_) ::EnableWindow(hEditLoadIter_, cfg_.gtest_load_enabled ? TRUE : FALSE);
    RefreshPairCombos();
    RefreshHistoryPage();
}

void App::SaveExtraUi()
{
    if (!hChkLogEnabled_) return;
    cfg_.log_enabled      = IsChecked(hChkLogEnabled_);
    cfg_.log_insert_temp  = IsChecked(hChkLogTemp_);
    cfg_.log_return_value = IsChecked(hChkLogRet_);
    cfg_.gtest_enabled    = IsChecked(hChkGTest_);
    cfg_.gtest_load_enabled = IsChecked(hChkLoadTest_);
    int li = _wtoi(util::Trim(GetEditText(hEditLoadIter_)).c_str());
    if (li < 0) li = 0;
    if (li > 1000000) li = 1000000;
    cfg_.gtest_load_iterations = li;
    int rs = (int)::SendMessageW(hCmbReportScope_, CB_GETCURSEL, 0, 0);
    cfg_.report_scope = (rs == 1) ? ReportScope::File :
                        (rs == 2) ? ReportScope::Project : ReportScope::Change;
    cfg_.log_source_cpp = ((int)::SendMessageW(hCmbLogExt_, CB_GETCURSEL, 0, 0) == 1);
    if (hEditLoadIter_) ::EnableWindow(hEditLoadIter_, cfg_.gtest_load_enabled ? TRUE : FALSE);
    cfgmgr_.SaveExtras(cfg_);
}

bool App::HandleExtraCommand(int id, int code, HWND ctrl)
{
    (void)ctrl;
    switch (id) {
    case IDC_CHK_LOADTEST:
    case IDC_CMB_LOGEXT:
        if (code == BN_CLICKED || code == CBN_SELCHANGE) SaveExtraUi();
        return true;
    case IDC_CHK_ADDENABLE:
        if (code == BN_CLICKED) {
            add_code_mode_ = IsChecked(hChkAddEnable_) ? true : false;
            SetStatus(add_code_mode_ ? L"追加コードモード: ON" : L"追加コードモード: OFF");
        }
        return true;
    case IDC_BTN_ADDSEARCH:
        if (code == BN_CLICKED) DoAddCodeSearch();
        return true;
    case IDC_BTN_HISTREFRESH:
        if (code == BN_CLICKED) { RefreshHistoryPage(); RefreshPairCombos(); }
        return true;
    case IDC_CMB_HISTRUN:
        if (code == CBN_SELCHANGE) RefreshHistoryPage();
        return true;
    case IDC_BTN_HISTRESTORE:
        if (code == BN_CLICKED) DoHistoryRestore();
        return true;
    case IDC_BTN_HISTCOMPARE:
        if (code == BN_CLICKED) DoHistoryCompare();
        return true;
    case IDC_BTN_HISTREPORT:
        if (code == BN_CLICKED) DoHistoryReport();
        return true;
    case IDC_BTN_UNDO:
        // 履歴タブでファイルを選んでいれば「その変更だけ」を戻す
        if (code == BN_CLICKED && HasHistorySelection()) { DoHistoryRestore(); return true; }
        return false;
    default:
        return false;
    }
}

void App::RefreshPairCombos()
{
    if (!hCmbPairFrom_) return;
    struct Item { HWND h; bool allow_none; };
    Item combos[6] = {
        { hCmbPairFrom_, false }, { hCmbPairTo_, false },
        { hCmbSysPairFrom_, false }, { hCmbSysPairTo_, false },
        { hCmbGtRangeFrom_, true }, { hCmbGtRangeTo_, true }
    };
    for (int c = 0; c < 6; ++c) {
        if (!combos[c].h) continue;
        DWORD_PTR keep = (DWORD_PTR)::SendMessageW(combos[c].h, CB_GETCURSEL, 0, 0);
        ::SendMessageW(combos[c].h, CB_RESETCONTENT, 0, 0);
        if (combos[c].allow_none)
            ::SendMessageW(combos[c].h, CB_ADDSTRING, 0, (LPARAM)L"指定なし（現在の検索結果）");
        if (history_runs_.empty()) {
            if (!combos[c].allow_none) {
                ::SendMessageW(combos[c].h, CB_ADDSTRING, 0, (LPARAM)L"（履歴なし）");
                ::SendMessageW(combos[c].h, CB_SETCURSEL, 0, 0);
            } else {
                ::SendMessageW(combos[c].h, CB_SETCURSEL, 0, 0);
            }
            continue;
        }
        int base = combos[c].allow_none ? 1 : 0;
        for (size_t i = 0; i < history_runs_.size(); ++i) {
            std::wstring lab = std::to_wstring((int)i + 1) + L": " +
                               HistoryManager::RunLabel(history_runs_[i]);
            ::SendMessageW(combos[c].h, CB_ADDSTRING, 0, (LPARAM)lab.c_str());
        }
        if (keep > 0 && keep < (DWORD_PTR)(base + (int)history_runs_.size()))
            ::SendMessageW(combos[c].h, CB_SETCURSEL, (WPARAM)keep, 0);
        else
            ::SendMessageW(combos[c].h, CB_SETCURSEL, 0, 0);
    }
}


bool App::GetPairIds(HWND from, HWND to, std::wstring& out_from, std::wstring& out_to) const
{
    out_from.clear();
    out_to.clear();
    if (!from || !to || history_runs_.empty()) return false;
    int a = (int)::SendMessageW(from, CB_GETCURSEL, 0, 0);
    int b = (int)::SendMessageW(to, CB_GETCURSEL, 0, 0);
    if (a < 0 || b < 0) return false;
    if (a >= (int)history_runs_.size() || b >= (int)history_runs_.size()) return false;
    out_from = history_runs_[a].id;
    out_to = history_runs_[b].id;
    return !out_from.empty() && !out_to.empty();
}

void App::RefreshHistoryPage()
{
    if (!hCmbHistRun_) return;
    history_runs_ = HistoryManager::ListRuns(HistoryBaseDir());
    ::SendMessageW(hCmbHistRun_, CB_RESETCONTENT, 0, 0);
    if (history_runs_.empty()) {
        ::SendMessageW(hCmbHistRun_, CB_ADDSTRING, 0, (LPARAM)L"（履歴がありません）");
        ::SendMessageW(hCmbHistRun_, CB_SETCURSEL, 0, 0);
        ::SendMessageW(hListHistFiles_, LVM_DELETEALLITEMS, 0, 0);
        if (hLblHistInfo_) ::SetWindowTextW(hLblHistInfo_, L"");
        return;
    }
    for (size_t i = 0; i < history_runs_.size(); ++i) {
        std::wstring lab = std::to_wstring((int)i + 1) + L": " +
                           HistoryManager::RunLabel(history_runs_[i]);
        ::SendMessageW(hCmbHistRun_, CB_ADDSTRING, 0, (LPARAM)lab.c_str());
    }
    int sel = (int)::SendMessageW(hCmbHistRun_, CB_GETCURSEL, 0, 0);
    if (sel < 0) { sel = 0; ::SendMessageW(hCmbHistRun_, CB_SETCURSEL, 0, 0); }

    ::SendMessageW(hListHistFiles_, LVM_DELETEALLITEMS, 0, 0);
    std::vector<HistoryFileEntry> files =
        HistoryManager::ListFiles(HistoryBaseDir(), history_runs_[sel].id);
    for (size_t i = 0; i < files.size(); ++i) {
        LVITEMW it;
        ::ZeroMemory(&it, sizeof(it));
        it.mask = LVIF_TEXT;
        it.iItem = (int)i;
        it.iSubItem = 0;
        std::wstring no = std::to_wstring((int)i + 1);
        it.pszText = (LPWSTR)no.c_str();
        ::SendMessageW(hListHistFiles_, LVM_INSERTITEMW, 0, (LPARAM)&it);
        LVITEMW sub;
        ::ZeroMemory(&sub, sizeof(sub));
        sub.mask = LVIF_TEXT;
        sub.iItem = (int)i;
        sub.iSubItem = 1;
        sub.pszText = (LPWSTR)files[i].path.c_str();
        ::SendMessageW(hListHistFiles_, LVM_SETITEMW, 0, (LPARAM)&sub);
        sub.iSubItem = 2;
        sub.pszText = (LPWSTR)(files[i].has_before ? L"あり" : L"-");
        ::SendMessageW(hListHistFiles_, LVM_SETITEMW, 0, (LPARAM)&sub);
        sub.iSubItem = 3;
        sub.pszText = (LPWSTR)(files[i].has_after ? L"あり" : L"-");
        ::SendMessageW(hListHistFiles_, LVM_SETITEMW, 0, (LPARAM)&sub);
    }
    if (hLblHistInfo_) {
        std::wstring info = history_runs_[sel].time + L"  " + history_runs_[sel].mode +
                            L"  " + std::to_wstring((int)files.size()) + L" ファイル";
        ::SetWindowTextW(hLblHistInfo_, info.c_str());
    }
}

bool App::HasHistorySelection() const
{
    HistoryFileEntry e;
    return SelectedHistoryFile(e) && e.has_before;
}

bool App::SelectedHistoryFile(HistoryFileEntry& out) const
{
    out = HistoryFileEntry();
    if (!hCmbHistRun_ || !hListHistFiles_) return false;
    int rs = (int)::SendMessageW(hCmbHistRun_, CB_GETCURSEL, 0, 0);
    if (rs < 0 || rs >= (int)history_runs_.size()) return false;
    int li = (int)::SendMessageW(hListHistFiles_, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (li < 0) return false;
    std::vector<HistoryFileEntry> files =
        HistoryManager::ListFiles(HistoryBaseDir(), history_runs_[rs].id);
    if (li >= (int)files.size()) return false;
    out = files[li];
    return true;
}

void App::DoHistoryRestore()
{
    HistoryFileEntry e;
    if (!SelectedHistoryFile(e) || !e.has_before) {
        ShowInfo(L"履歴タブで対象ファイルを選択してください。\n"
                 L"（変更前スナップショットがある行だけ元に戻せます）");
        return;
    }
    std::wstring msg = L"この変更だけを元に戻します。よろしいですか？\n\n" + e.path +
                       L"\n\n復元元: " + e.before;
    if (!Confirm(msg)) return;

    std::wstring base = HistoryBaseDir();
    std::wstring side = HistoryManager::SidecarDirectory(base);
    util::EnsureDirectory(side);
    std::wstring err;
    std::wstring sp = util::JoinPath(side, util::GetFileNameOf(e.path) + L"." +
                                     util::TimestampFileName() + L".undobak");
    bool had_current = util::PathExists(e.path);
    if (had_current) FileManager::CopyFileTo(e.path, sp, err);

    if (!FileManager::CopyFileTo(e.before, e.path, err)) {
        ShowError(L"復元に失敗しました。\n" + err);
        return;
    }

    // 復元も履歴として残す（連続して戻せるように）
    if (had_current) {
        HistoryFileEntry h;
        h.path = e.path;
        h.before = sp;
        h.has_before = util::PathExists(sp);
        h.after = e.before;
        h.has_after = true;
        std::vector<HistoryFileEntry> v;
        v.push_back(h);
        HistoryRunInfo run;
        if (HistoryManager::Record(base, util::NewOperationId(), L"Undo", 1, v, run, err)) {
            log_.Info(L"Undo recorded: " + run.id);
        }
    }

    if (!IsFolderMode()) {
        target_path_ = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
        FileManager::ReadText(target_path_, cfg_.encoding_mode, file_text_, file_info_);
    }
    matches_.clear();
    RefreshListView();
    RefreshHistoryPage();
    RefreshPairCombos();
    ShowInfo(L"指定した変更だけを元に戻しました。\n（退避: " + sp + L"）");
}

void App::DoHistoryCompare()
{
    HistoryFileEntry e;
    if (!SelectedHistoryFile(e)) {
        ShowInfo(L"履歴タブで比較するファイルを選択してください。");
        return;
    }
    std::wstring exe = FindWinMergePath();
    if (exe.empty()) { ShowError(L"WinMerge が見つかりません。検索条件タブで場所を指定してください。"); return; }
    if (!e.has_before || !e.has_after) { ShowError(L"比較できるスナップショットがありません。"); return; }
    std::wstring tb = TempSnapshotPath(L"b", 1), ta = TempSnapshotPath(L"a", 2);
    std::wstring err;
    FileManager::CopyFileTo(e.before, tb, err);
    FileManager::CopyFileTo(e.after, ta, err);
    std::wstring cmd = L"\"" + tb + L"\" \"" + ta + L"\"";
    ::ShellExecuteW(nullptr, L"open", exe.c_str(), cmd.c_str(), nullptr, SW_SHOWNORMAL);
}

void App::DoHistoryReport()
{
    if (!hCmbHistRun_) return;
    int rs = (int)::SendMessageW(hCmbHistRun_, CB_GETCURSEL, 0, 0);
    if (rs < 0 || rs >= (int)history_runs_.size()) {
        ShowInfo(L"履歴がありません。");
        return;
    }
    BuildPairReport(history_runs_[rs].id, history_runs_[rs].id, L"history");
}

bool App::BuildPairCompareItems(const std::wstring& from_id, const std::wstring& to_id,
                                std::vector<CompareItem>& out, std::wstring& out_error)
{
    out.clear();
    out_error.clear();
    std::wstring base = HistoryBaseDir();
    std::vector<HistoryFileEntry> rf = HistoryManager::ListFiles(base, from_id);
    std::vector<HistoryFileEntry> rt = HistoryManager::ListFiles(base, to_id);
    if (rf.empty() || rt.empty()) {
        out_error = L"指定した範囲のスナップショットがありません。";
        return false;
    }
    for (size_t i = 0; i < rf.size(); ++i) {
        const HistoryFileEntry* b = nullptr;
        for (size_t j = 0; j < rt.size(); ++j)
            if (rt[j].path == rf[i].path) { b = &rt[j]; break; }
        if (!b || !rf[i].has_before || !b->has_after) continue;
        std::wstring tb, ta, err;
        if (!HistoryManager::LoadText(rf[i].before, tb, err)) continue;
        if (!HistoryManager::LoadText(b->after, ta, err)) continue;
        if (tb == ta) continue;
        CompareItem c;
        c.path = rf[i].path;
        c.before = tb;
        c.after = ta;
        out.push_back(c);
    }
    if (out.empty()) { out_error = L"指定した範囲に差分がありません。"; return false; }
    return true;
}

bool App::BuildPairReport(const std::wstring& from_id, const std::wstring& to_id,
                          const std::wstring& tag)
{
    std::wstring err;
    std::vector<CompareItem> items;
    if (!BuildPairCompareItems(from_id, to_id, items, err)) { ShowInfo(err); return false; }

    std::wstring md;
    md += L"# 変更範囲レポート\n\n";
    md += L"- 変更前: " + from_id + L"\n";
    md += L"- 変更後: " + to_id + L"\n";
    md += L"- 作成日時: " + util::NowDateTime() + L"\n";
    md += L"- 対象ファイル: " + std::to_wstring((int)items.size()) + L" 件\n\n";

    for (size_t i = 0; i < items.size(); ++i) {
        md += L"## " + items[i].path + L"\n\n";
        md += L"```diff\n";
        md += HistoryManager::BuildDiffText(items[i].before, items[i].after, 400);
        md += L"```\n\n";
    }

    std::wstring esc;
    for (size_t i = 0; i < md.size(); ++i) {
        wchar_t c = md[i];
        if (c == L'<') esc += L"&lt;";
        else if (c == L'>') esc += L"&gt;";
        else if (c == L'&') esc += L"&amp;";
        else esc.push_back(c);
    }
    std::wstring html = L"<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
                        L"<title>変更範囲レポート</title><style>body{font-family:sans-serif;"
                        L"margin:24px}pre{background:#f6f8fa;padding:12px;overflow:auto}</style>"
                        L"</head><body><pre>" + esc + L"</pre></body></html>";

    std::wstring dir = ReportDirectory();
    util::EnsureDirectory(dir);
    std::wstring stem = L"pair_" + tag + L"_" + from_id + L"_to_" + to_id + L"_" +
                        util::TimestampFileName();
    std::wstring mdPath = util::JoinPath(dir, stem + L".md");
    std::wstring htmlPath = util::JoinPath(dir, stem + L".html");
    if (!FileManager::WriteTextAtomic(mdPath, md, TextEncoding::UTF8, true, NewlineKind::CRLF, err)) {
        ShowError(L"レポートを書き出せません。\n" + err);
        return false;
    }
    FileManager::WriteTextAtomic(htmlPath, html, TextEncoding::UTF8, true, NewlineKind::CRLF, err);
    last_report_path_ = htmlPath;
    log_.Info(L"Pair report: " + htmlPath);
    ShowInfo(L"比較レポートを作成しました。\n" + htmlPath);
    OpenPath(htmlPath);
    return true;
}

void App::CaptureBeforeSnapshot(std::vector<HistoryFileEntry>& out)
{
    out.clear();
    std::wstring err;
    if (!IsFolderMode()) {
        if (target_path_.empty() || !util::PathExists(target_path_)) return;
        HistoryFileEntry e;
        e.path = target_path_;
        e.before = TempSnapshotPath(L"b", (int)out.size() + 1);
        if (FileManager::CopyFileTo(target_path_, e.before, err)) {
            e.has_before = true;
            out.push_back(e);
        }
        return;
    }
    for (size_t i = 0; i < batch_entries_.size(); ++i) {
        if (!batch_entries_[i].selected) continue;
        if (!util::PathExists(batch_entries_[i].path)) continue;
        HistoryFileEntry e;
        e.path = batch_entries_[i].path;
        e.before = TempSnapshotPath(L"b", (int)out.size() + 1);
        if (FileManager::CopyFileTo(e.path, e.before, err)) {
            e.has_before = true;
            out.push_back(e);
        }
    }
}

void App::RecordHistoryAfterRun(const std::vector<HistoryFileEntry>& captured,
                                const std::wstring& mode)
{
    if (captured.empty()) return;
    std::vector<HistoryFileEntry> changed;
    std::wstring err;
    for (size_t i = 0; i < captured.size(); ++i) {
        HistoryFileEntry e = captured[i];
        if (!util::PathExists(e.path)) continue;
        e.after = TempSnapshotPath(L"a", (int)i + 1000);
        if (!FileManager::CopyFileTo(e.path, e.after, err)) continue;
        e.has_after = true;
        if (e.has_before && FilesEqual(e.before, e.after)) {
            ::DeleteFileW(e.after.c_str());
            continue;
        }
        changed.push_back(e);
    }
    if (changed.empty()) return;

    HistoryRunInfo run;
    if (!HistoryManager::Record(HistoryBaseDir(), util::NewOperationId(), mode,
                                (int)changed.size(), changed, run, err)) {
        log_.Warn(L"履歴の記録に失敗しました: " + err);
        return;
    }
    log_.Info(L"履歴を記録しました: " + run.id);
    history_runs_.insert(history_runs_.begin(), run);
    RefreshHistoryPage();
    RefreshPairCombos();
}

void App::ReadAddCodeInputs()
{
    add_before_ = util::NormalizeNewlines(GetEditText(hEditAddBefore_));
    add_after_  = util::NormalizeNewlines(GetEditText(hEditAddAfter_));
    add_code_   = util::NormalizeNewlines(GetEditText(hEditAddCode_));
    while (!add_before_.empty() && (add_before_[add_before_.size() - 1] == L'\n' ||
                                    add_before_[add_before_.size() - 1] == L'\r'))
        add_before_.erase(add_before_.size() - 1);
    while (!add_after_.empty() && (add_after_[add_after_.size() - 1] == L'\n' ||
                                   add_after_[add_after_.size() - 1] == L'\r'))
        add_after_.erase(add_after_.size() - 1);
}

// needle の非空白文字を、text の pos 以降で空白を読み飛ばしながら順に照合する
static bool NormalizedStartsWith(const std::wstring& text, size_t pos, const std::wstring& needle)
{
    size_t i = pos, k = 0;
    while (k < needle.size()) {
        while (k < needle.size() && (needle[k] == L' ' || needle[k] == L'\t' ||
                                     needle[k] == L'\r' || needle[k] == L'\n')) ++k;
        if (k >= needle.size()) break;
        while (i < text.size() && (text[i] == L' ' || text[i] == L'\t' ||
                                   text[i] == L'\r' || text[i] == L'\n')) ++i;
        if (i >= text.size()) return false;
        if (text[i] != needle[k]) return false;
        ++i; ++k;
    }
    return true;
}

int App::CollectAddMatches(const std::wstring& text, const std::wstring& file_path,
                           std::vector<MatchResult>& out) const
{
    if (add_before_.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t p = text.find(add_before_, pos);
        if (p == std::wstring::npos) break;
        size_t ap = p + add_before_.size();
        // 「直後のコード」は空白・改行の違いを無視して照合する
        bool ok = add_after_.empty() || NormalizedStartsWith(text, ap, add_after_);
        if (ok) {
            MatchResult m;
            m.match_id = (int)out.size();
            m.file_path = file_path;
            m.start_offset = p;
            m.end_offset = ap;
            m.matched_text = add_before_;
            int line = 1, col = 1;
            for (size_t k = 0; k < p && k < text.size(); ++k) {
                if (text[k] == L'\n') { ++line; col = 1; } else ++col;
            }
            m.start_line = line;
            m.start_column = col;
            m.end_line = line;
            m.end_column = col + (int)add_before_.size();
            m.selected = true;
            m.match_rate = 100;
            out.push_back(m);
            ++count;
        }
        pos = ap;
    }
    return count;
}

void App::DoAddCodeSearch()
{
    ReadAddCodeInputs();
    add_code_mode_ = IsChecked(hChkAddEnable_) ? true : false;
    if (add_before_.empty()) {
        ShowError(L"「直前のコード」を入力してください。");
        return;
    }
    if (add_code_.empty()) {
        ShowError(L"「追加するコード」を入力してください。");
        return;
    }

    matches_.clear();
    int total = 0;
    if (!IsFolderMode()) {
        target_path_ = util::MakeAbsolute(util::Trim(GetEditText(hTargetEdit_)));
        if (target_path_.empty() || !FileManager::ReadText(target_path_, cfg_.encoding_mode,
                                                           file_text_, file_info_)) {
            ShowError(L"対象ファイルを読み取れません。");
            return;
        }
        total = CollectAddMatches(file_text_, target_path_, matches_);
    } else {
        for (size_t i = 0; i < batch_entries_.size(); ++i) {
            BatchFileEntry& e = batch_entries_[i];
            if (!e.selected) continue;
            total += CollectAddMatches(e.text, e.path, matches_);
            e.hit_count = total;
        }
    }

    RefreshListView();
    UpdateSelectionColumn();
    std::wstring msg = L"追加位置: " + std::to_wstring(total) + L" 件";
    if (hLblAddInfo_) ::SetWindowTextW(hLblAddInfo_, msg.c_str());
    SetStatus(msg);
    SetState(AppState::SEARCHED);   // プレビュー / ドライラン / 実行を有効化
    log_.Info(L"追加コード検索: " + msg);
    if (total == 0) ShowInfo(L"追加位置が見つかりませんでした。\n「直前のコード」と「直後のコード」を確認してください。");
}
} // namespace ms

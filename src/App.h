// =============================================================================
// App.h
// Migration Switcher - アプリケーション本体 / UIManager（設計書 7.2.1）
// Phase 4: 17章 複数ファイル一括処理 対応
// =============================================================================
#pragma once

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#include "Models.h"
#include "AiClient.h"
#include "SpecGenerator.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "SwitchHeaderManager.h"
#include "UndoManager.h"

namespace ms {

// タブの並び（設計書 5.1 / 17.2.1）
enum PageIndex {
    PG_BASIC   = 0,   // 基本設定
    PG_BATCH   = 1,   // 一括処理（Phase 4）
    PG_OLD     = 2,   // 旧コード
    PG_NEW     = 3,   // 新コード
    PG_RESULT  = 4,   // 検索結果
    PG_PREVIEW = 5,   // プレビュー
    PG_SWITCH  = 6,   // スイッチ管理
    PG_SYSTEM  = 7,   // システム情報（仕様書生成）
    PG_LOG     = 8,   // ログ
    PG_COUNT   = 9
};

class App {
public:
    App();
    ~App();

    bool Initialize(HINSTANCE hInst, int nCmdShow);
    int  Run();

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    // タブページ用。ページ上のコントロールの WM_COMMAND / WM_NOTIFY を
    // アプリ本体（hWnd_）と同じハンドラへ転送するために必要。
    static LRESULT CALLBACK PageWndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    // ---- 構築 ----
    bool RegisterWindowClass();
    bool CreateMainWindow(int nCmdShow);
    void CreateFonts();
    void CreateControls();
    void CreateBasicPage();
    void CreateBatchPage();
    void CreateOldCodePage();
    void CreateNewCodePage();
    void CreateResultPage();
    void CreatePreviewPage();
    void CreateSwitchPage();
    void CreateSystemPage();
    void CreateLogPage();
    void ApplyFont(HWND h, bool mono = false);
    void LoadConfigToUi();
    void SaveUiToConfig();
    void LayoutControls();
    void LayoutPage(int index, const RECT& rc);
    void UpdateInputInfo();

    // ---- イベント ----
    void OnCreate();
    void OnSize(int cx, int cy);
    void OnCommand(int id, int code, HWND ctrl);
    void OnNotify(NMHDR* nm);
    void OnTabChange();
    void OnDestroy();
    void OnDropFiles(HDROP hDrop);

    // ---- 操作 ----
    void DoBrowse();
    void DoFolderBrowse();
    void DoScan();
    void DoSearch();
    void DoBatchSearch();
    void DoPreview();
    void DoDryRun();
    void DoExecute();
    void DoUndo();
    void DoOpenHtml();
    void DoSelectAll();
    void DoClearSelection();
    void DoFileSelectAll();
    void DoFileSelectNone();
    void DoClearLog();
    void DoPreviewModeChanged();
    void OnTargetKindChanged();
    // ---- スイッチ管理 ----
    void DoSwitchHeaderBrowse();
    void DoSwitchHeaderDirBrowse();
    void DoSwitchHeaderLoad();
    void DoSwitchEnable();
    void DoSwitchDisable();
    void DoSwitchRemove();
    void DoSwitchOpen();
    void DoSwitchCreateNow();
    void RefreshSwitchList();
    void RefreshResolvedHeaderPath();
    std::wstring CurrentSwitchHeaderPath() const;
    std::wstring SwitchHeaderBaseDir() const;
    bool PrepareSwitchHeader(const OperationContext& ctx,
                             const std::vector<std::wstring>& source_files,
                             bool backup_header,
                             bool& out_added,
                             std::wstring& out_backup,
                             std::wstring& out_error);
    bool ApplyIncludeToText(std::wstring& text,
                            const std::wstring& source_path,
                            const OperationContext& ctx,
                            bool& out_added,
                            int& out_line);

    // ---- AI 連携（変更仕様書 / 検査仕様書）----
    void DoAiTest();
    void DoAiProviderChanged();
    void LoadAiFieldsFromConfig(int provider);
    void SaveAiFieldsToConfig(int provider);
    void DoGenerateSpec(int kind);
    void DoGenerateSpecBoth();
    void DoOpenSpecReport();
    void DoExportPrompt(int kind);
    void DoExportPromptBoth();
    void DoMakeReportFromText(int kind);
    bool GenerateSpecs(bool do_change, bool do_inspect);
    bool ExportPrompts(bool do_change, bool do_inspect);
    AiSettings  ReadAiSettings() const;
    SpecInput   BuildSpecInput() const;
    void PumpMessages();

    // ---- 内部処理 ----
    void ReadInputs(OperationContext& ctx);
    bool CheckInputs(bool for_execute, std::wstring& out_error,
                     std::vector<std::wstring>& out_warnings);
    void EvaluateMatchWarnings();
    void ApplyHitBehavior();
    void RefreshListView();
    void RefreshFileList();
    void SyncSelectionFromList();
    void SyncFileSelectionFromList();
    void UpdateSelectionColumn();
    std::vector<MatchResult> GetSelectedMatches() const;
    std::vector<std::wstring> GetSelectedFiles() const;
    bool BuildPreviewInternal(std::wstring& out_error);
    bool BuildBatchPreview(std::wstring& out_error);
    void RunOperation(ExecutionMode mode);
    void RunBatchOperation(ExecutionMode mode);
    std::wstring MakeBlockFor(const MatchResult& m, const OperationContext& ctx,
                              const std::wstring& text) const;
    std::vector<std::wstring> MakeBlocksFor(const std::vector<MatchResult>& ms,
                                            const OperationContext& ctx,
                                            const std::wstring& text) const;
    void SetState(AppState s);
    void UpdateButtons();
    void SetStatus(const std::wstring& s);
    void AppendLog(const std::wstring& line);
    void ShowError(const std::wstring& m);
    void ShowWarn(const std::wstring& m);
    void ShowInfo(const std::wstring& m);
    bool Confirm(const std::wstring& m);
    void SetProgress(int percent);
    void OpenPath(const std::wstring& p);
    SwitchStyle ReadStyleFromUi() const;
    HitBehavior ReadHitBehaviorFromUi() const;
    PreviewMode ReadPreviewModeFromUi() const;
    TargetKind  ReadTargetKindFromUi() const;
    std::vector<std::wstring> ReadExtensionsFromUi() const;
    std::wstring ReportDirectory() const;
    std::wstring BackupDirectory() const;
    std::wstring GetEditText(HWND h) const;
    void SetEditText(HWND h, const std::wstring& s);
    bool IsChecked(HWND h) const;
    void SetChecked(HWND h, bool v);
    void SetRadioGroup(HWND* group, int count, int index);
    bool IsFolderMode() const;
    void RefreshScanInfo();

    // ---- メンバ ----
    HINSTANCE hInst_;
    HWND      hWnd_;
    HWND      hStatus_;
    HWND      hProgress_;

    HWND hLblFile_, hTargetEdit_, hBrowseBtn_;
    HWND hLblSwitch_, hSwitchEdit_, hLblHintSwitch_;
    HWND hTab_;
    HWND hPage_[PG_COUNT];

    // page PG_BASIC: 基本設定
    HWND hGrpStyle_, hRadStyle_[4], hLblTemplate_, hEditTemplate_;
    HWND hGrpOption_, hChkBackup_, hChkReport_, hChkLog_, hChkOpenReport_;
    HWND hChkDryRunOnly_, hChkAutoIndent_, hChkWarnComment_, hChkWarnPreproc_;
    HWND hGrpHit_, hRadHit_[4];

    // page PG_BATCH: 一括処理（設計書 17.2.1 追加UI）
    HWND hGrpTargetKind_, hRadTargetKind_[2];
    HWND hLblFolder_, hFolderEdit_, hFolderBrowseBtn_;
    HWND hGrpExtFilter_, hChkExt_[6], hChkSubdirs_;
    HWND hBtnScan_, hBtnFileSelAll_, hBtnFileSelNone_, hLblScanInfo_;
    HWND hListFiles_;

    // page PG_OLD / PG_NEW
    HWND hOldEdit_, hLblOldInfo_;
    HWND hNewEdit_, hLblNewInfo_;

    // page PG_RESULT
    HWND hListView_, hLblResultInfo_, hBtnSelectAll_, hBtnClearSel_;

    // page PG_PREVIEW
    HWND hPreviewEdit_, hCmbPreviewMode_, hBtnOpenHtml_;

    // page PG_SWITCH: スイッチ管理
    HWND hLblSwHeader_, hSwHeaderEdit_, hBtnSwHeaderBrowse_, hBtnSwHeaderLoad_;
    HWND hGrpSwAuto_, hChkManageSwHeader_, hChkAddInclude_;
    HWND hLblSwHeaderName_, hEditSwHeaderName_;
    HWND hLblSwHeaderDir_, hEditSwHeaderDir_, hBtnSwHeaderDir_;
    HWND hLblSwMemo_, hEditSwMemo_;
    HWND hLblSwInfo_, hListSwitches_;
    HWND hBtnSwEnable_, hBtnSwDisable_, hBtnSwRemove_, hBtnSwOpen_, hBtnSwCreate_;
    SwitchHeaderInfo sw_header_info_;

    // page PG_SYSTEM: システム情報（仕様書生成）
    HWND hLblSysName_, hEditSysName_;
    HWND hLblSysOverview_, hEditSysOverview_;
    HWND hLblSysPurpose_, hEditSysPurpose_;
    HWND hLblSysMod_, hEditSysMod_;
    HWND hLblSysExtra_, hEditSysExtra_;
    HWND hBtnSpecChange_, hBtnSpecInspect_, hBtnSpecBoth_, hBtnSpecOpen_;
    HWND hChkAiOpenReport_, hEditSpecOut_, hLblSpecInfo_, hLblSpecOutNote_;
    HWND hBtnPromptChange_, hBtnPromptInspect_, hBtnPromptBoth_;
    HWND hBtnMakeChange_, hBtnMakeInspect_, hLblPasteNote_;

    // page PG_BASIC: AI 連携
    HWND hGrpAi_, hLblAiProvider_, hCmbAiProvider_, hLblAiKey_, hEditAiKey_, hBtnAiTest_;
    HWND hLblAiEndpoint_, hEditAiEndpoint_;
    HWND hLblAiModel_, hEditAiModel_, hLblAiTimeout_, hEditAiTimeout_;

    // page PG_LOG
    HWND hLogEdit_, hBtnClearLog_;

    HWND hBtnSearch_, hBtnPreview_, hBtnDryRun_, hBtnExecute_, hBtnUndo_, hBtnExit_;

    HFONT hFontUi_, hFontMono_, hFontBold_;

    ConfigManager cfgmgr_;
    LogManager    log_;
    UndoManager   undo_;

    AppConfig cfg_;
    AppState  state_;

    // 単一ファイルモード
    std::wstring file_text_;       // LF 正規化済み内部表現
    FileInfo     file_info_;
    std::wstring target_path_;

    // 一括処理モード（Phase 4）
    std::vector<BatchFileEntry> batch_entries_;
    std::vector<std::wstring>   batch_files_;
    BatchExecuteResult          last_batch_result_;
    std::wstring                batch_folder_;

    // 共通
    std::vector<MatchResult> matches_;
    ReplaceResult last_result_;
    std::wstring  last_report_path_;
    std::wstring  last_backup_path_;
    bool          suppress_events_;
    bool          generating_;
    std::wstring  last_spec_report_path_;

    static App* s_instance_;
};

} // namespace ms

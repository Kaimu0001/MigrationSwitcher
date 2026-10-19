// =============================================================================
// UIManager.cpp
// 設計書 5章「画面設計」/ 17.2.1「追加UI」/ 7.2.1 UIManager
// =============================================================================
#include "App.h"
#include "resource.h"
#include "Utils.h"
#include "FileManager.h"
#include "FolderScanner.h"
#include "ValidationManager.h"

#include <shellapi.h>
#include <cstdio>

namespace ms {

App* App::s_instance_ = nullptr;

static const wchar_t* kMainClass = L"MS_MainWindowClass";
static const wchar_t* kPageClass = L"MS_PageWindowClass";

// -----------------------------------------------------------------------------
// 生成ヘルパ
// -----------------------------------------------------------------------------
// winuser.h の IDC_ARROW / IDI_APPLICATION は UNICODE の有無によって
// LPSTR / LPWSTR のどちらへ展開されるかが変わる。VS2022 のプロジェクトで
// UNICODE が未定義でもコンパイルできるよう、ワイド版を明示的に使う。
static LPCWSTR kIdcArrow       = MAKEINTRESOURCEW(32512);   // IDC_ARROW
static LPCWSTR kIdiApplication = MAKEINTRESOURCEW(32512);   // IDI_APPLICATION
static void SetWndFont(HWND h, HFONT f)
{
    if (h) ::SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}

static HWND MakeStatic(HWND parent, const wchar_t* text, int id, HFONT font)
{
    HWND h = ::CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

static HWND MakeButton(HWND parent, const wchar_t* text, int id, DWORD style, HFONT font)
{
    HWND h = ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

static HWND MakeCheck(HWND parent, const wchar_t* text, int id, HFONT font)
{
    HWND h = ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

static HWND MakeRadio(HWND parent, const wchar_t* text, int id, DWORD extra, HFONT font)
{
    HWND h = ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | extra,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

static HWND MakeGroup(HWND parent, const wchar_t* text, int id, HFONT font)
{
    HWND h = ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

static HWND MakeEdit(HWND parent, int id, DWORD style, HFONT font)
{
    HWND h = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | style,
        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SetWndFont(h, font);
    return h;
}

// -----------------------------------------------------------------------------
// ライフサイクル
// -----------------------------------------------------------------------------
App::App()
    : hInst_(nullptr), hWnd_(nullptr), hStatus_(nullptr), hProgress_(nullptr),
      hLblFile_(nullptr), hTargetEdit_(nullptr), hBrowseBtn_(nullptr),
      hLblSwitch_(nullptr), hSwitchEdit_(nullptr), hLblHintSwitch_(nullptr),
      hTab_(nullptr),
      hGrpStyle_(nullptr), hLblTemplate_(nullptr), hEditTemplate_(nullptr),
      hGrpOption_(nullptr), hChkBackup_(nullptr), hChkReport_(nullptr),
      hChkLog_(nullptr), hChkOpenReport_(nullptr), hChkDryRunOnly_(nullptr),
      hChkAutoIndent_(nullptr), hChkWarnComment_(nullptr), hChkWarnPreproc_(nullptr),
      hGrpHit_(nullptr),
      hGrpTargetKind_(nullptr), hLblFolder_(nullptr), hFolderEdit_(nullptr),
      hFolderBrowseBtn_(nullptr), hGrpExtFilter_(nullptr), hChkSubdirs_(nullptr),
      hBtnScan_(nullptr), hBtnFileSelAll_(nullptr), hBtnFileSelNone_(nullptr),
      hLblScanInfo_(nullptr), hListFiles_(nullptr),
      hOldEdit_(nullptr), hLblOldInfo_(nullptr),
      hNewEdit_(nullptr), hLblNewInfo_(nullptr),
      hListView_(nullptr), hLblResultInfo_(nullptr),
      hBtnSelectAll_(nullptr), hBtnClearSel_(nullptr),
      hPreviewEdit_(nullptr), hCmbPreviewMode_(nullptr), hBtnOpenHtml_(nullptr),
      hLblSwHeader_(nullptr), hSwHeaderEdit_(nullptr), hBtnSwHeaderBrowse_(nullptr),
      hBtnSwHeaderLoad_(nullptr), hGrpSwAuto_(nullptr), hChkManageSwHeader_(nullptr),
      hChkAddInclude_(nullptr), hLblSwHeaderName_(nullptr), hEditSwHeaderName_(nullptr),
      hLblSwHeaderDir_(nullptr), hEditSwHeaderDir_(nullptr), hBtnSwHeaderDir_(nullptr),
      hLblSwMemo_(nullptr), hEditSwMemo_(nullptr), hLblSwInfo_(nullptr),
      hListSwitches_(nullptr), hBtnSwEnable_(nullptr), hBtnSwDisable_(nullptr),
      hBtnSwRemove_(nullptr), hBtnSwOpen_(nullptr), hBtnSwCreate_(nullptr),
      hLblSysName_(nullptr), hEditSysName_(nullptr),
      hLblSysOverview_(nullptr), hEditSysOverview_(nullptr),
      hLblSysPurpose_(nullptr), hEditSysPurpose_(nullptr),
      hLblSysMod_(nullptr), hEditSysMod_(nullptr),
      hLblSysExtra_(nullptr), hEditSysExtra_(nullptr),
      hBtnSpecChange_(nullptr), hBtnSpecInspect_(nullptr), hBtnSpecBoth_(nullptr),
      hBtnSpecOpen_(nullptr), hChkAiOpenReport_(nullptr), hEditSpecOut_(nullptr),
      hLblSpecInfo_(nullptr), hLblSpecOutNote_(nullptr),
      hBtnPromptChange_(nullptr), hBtnPromptInspect_(nullptr), hBtnPromptBoth_(nullptr),
      hBtnMakeChange_(nullptr), hBtnMakeInspect_(nullptr), hLblPasteNote_(nullptr),
      hGrpAi_(nullptr), hLblAiProvider_(nullptr), hCmbAiProvider_(nullptr),
      hLblAiKey_(nullptr), hEditAiKey_(nullptr), hBtnAiTest_(nullptr),
      hLblAiEndpoint_(nullptr), hEditAiEndpoint_(nullptr),
      hLblAiModel_(nullptr), hEditAiModel_(nullptr),
      hLblAiTimeout_(nullptr), hEditAiTimeout_(nullptr),
      hLogEdit_(nullptr), hBtnClearLog_(nullptr),
      hBtnSearch_(nullptr), hBtnPreview_(nullptr), hBtnDryRun_(nullptr),
      hBtnExecute_(nullptr), hBtnUndo_(nullptr), hBtnExit_(nullptr),
      hFontUi_(nullptr), hFontMono_(nullptr), hFontBold_(nullptr),
      state_(AppState::INIT), suppress_events_(false), generating_(false)
{
    for (int i = 0; i < PG_COUNT; ++i) hPage_[i] = nullptr;
    for (int i = 0; i < 4; ++i) { hRadStyle_[i] = nullptr; hRadHit_[i] = nullptr; }
    for (int i = 0; i < 2; ++i) hRadTargetKind_[i] = nullptr;
    for (int i = 0; i < 6; ++i) hChkExt_[i] = nullptr;
}

App::~App()
{
    if (hFontUi_)   ::DeleteObject(hFontUi_);
    if (hFontMono_) ::DeleteObject(hFontMono_);
    if (hFontBold_) ::DeleteObject(hFontBold_);
}

bool App::Initialize(HINSTANCE hInst, int nCmdShow)
{
    hInst_ = hInst;
    s_instance_ = this;

    cfgmgr_.SetPath(ConfigManager::DefaultIniPath());
    cfgmgr_.Load(cfg_);
    if (cfg_.default_report_directory.empty()) cfg_.default_report_directory = L"reports";
    if (cfg_.default_backup_directory.empty()) cfg_.default_backup_directory = L"backups";
    if (cfg_.encoding_mode.empty()) cfg_.encoding_mode = L"auto";
    if (cfg_.extension_filter.empty()) cfg_.extension_filter = FolderScanner::DefaultExtensions();

    log_.Initialize(util::ModuleDirectory(), cfg_.create_log);
    log_.SetSink([this](const std::wstring& line) { AppendLog(line); });

    if (!RegisterWindowClass()) return false;
    if (!CreateMainWindow(nCmdShow)) return false;
    return true;
}

int App::Run()
{
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (hWnd_ && ::IsDialogMessageW(hWnd_, &msg)) continue;
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK App::WndProcStatic(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    App* self = s_instance_;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (App*)cs->lpCreateParams;
        if (!self) self = s_instance_;
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
        if (self) { self->hWnd_ = hWnd; s_instance_ = self; }
    } else {
        self = (App*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }
    if (!self) return ::DefWindowProcW(hWnd, msg, wp, lp);
    return self->WndProc(hWnd, msg, wp, lp);
}

// -----------------------------------------------------------------------------
// タブページのウィンドウプロシージャ
//   ページ上のコントロール（ボタン / ラジオ / チェック / ListView）が送る
//   WM_COMMAND と WM_NOTIFY は「親ウィンドウ」宛に届く。ページの親は hWnd_ では
//   なくページ自身なので、ここで受け取ってアプリ本体のハンドラへ転送する。
//   （転送しないとページ上のボタンやラジオが無反応になる）
// -----------------------------------------------------------------------------
LRESULT CALLBACK App::PageWndProcStatic(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        self = (App*)cs->lpCreateParams;
        if (!self) self = s_instance_;
        ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (App*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }
    if (!self) return ::DefWindowProcW(hWnd, msg, wp, lp);

    switch (msg) {
    case WM_COMMAND:
        self->OnCommand(LOWORD(wp), HIWORD(wp), (HWND)lp);
        return 0;
    case WM_NOTIFY:
        self->OnNotify((NMHDR*)lp);
        return 0;
    case WM_ERASEBKGND: {
        // 背景を自前で塗る（初回表示時に未描画のまま白く残るのを防ぐ）
        HDC dc = (HDC)wp;
        RECT rc;
        ::GetClientRect(hWnd, &rc);
        ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_3DFACE));
        return 1;
    }
    default:
        break;
    }
    return ::DefWindowProcW(hWnd, msg, wp, lp);
}

bool App::RegisterWindowClass()
{
    WNDCLASSEXW wc;
    ::ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = App::WndProcStatic;
    wc.hInstance     = hInst_;
    wc.hCursor       = ::LoadCursorW(nullptr, kIdcArrow);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = kMainClass;
    wc.hIcon         = ::LoadIconW(nullptr, kIdiApplication);
    wc.hIconSm       = ::LoadIconW(nullptr, kIdiApplication);
    if (!::RegisterClassExW(&wc)) return false;

    WNDCLASSEXW pc;
    ::ZeroMemory(&pc, sizeof(pc));
    pc.cbSize        = sizeof(pc);
    pc.style         = CS_HREDRAW | CS_VREDRAW;
    pc.lpfnWndProc   = App::PageWndProcStatic;
    pc.hInstance     = hInst_;
    pc.hCursor       = ::LoadCursorW(nullptr, kIdcArrow);
    pc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    pc.lpszClassName = kPageClass;
    if (!::RegisterClassExW(&pc)) return false;
    return true;
}

bool App::CreateMainWindow(int nCmdShow)
{
    hWnd_ = ::CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kMainClass,
        L"CopyMigration Switcher - Migration Switch Tool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1240, 860,
        nullptr, nullptr, hInst_, this);
    if (!hWnd_) return false;
    ::ShowWindow(hWnd_, nCmdShow);
    ::UpdateWindow(hWnd_);
    return true;
}

void App::CreateFonts()
{
    NONCLIENTMETRICSW ncm;
    ::ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    LOGFONTW lf;
    if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
        lf = ncm.lfMessageFont;
    } else {
        ::ZeroMemory(&lf, sizeof(lf));
        lf.lfHeight = -12;
        lstrcpyW(lf.lfFaceName, L"MS Shell Dlg 2");
    }
    hFontUi_ = ::CreateFontIndirectW(&lf);

    LOGFONTW mf = lf;
    mf.lfHeight = -13;
    mf.lfWeight = FW_NORMAL;
    mf.lfCharSet = SHIFTJIS_CHARSET;
    lstrcpyW(mf.lfFaceName, L"Consolas");
    hFontMono_ = ::CreateFontIndirectW(&mf);

    LOGFONTW bf = lf;
    bf.lfWeight = FW_BOLD;
    hFontBold_ = ::CreateFontIndirectW(&bf);
}

void App::ApplyFont(HWND h, bool mono)
{
    SetWndFont(h, mono ? hFontMono_ : hFontUi_);
}

// -----------------------------------------------------------------------------
// 9.1 アプリ起動時フロー
// -----------------------------------------------------------------------------
void App::OnCreate()
{
    // 初回表示を確実に描画する
    LayoutControls();
    ::InvalidateRect(hWnd_, nullptr, TRUE);
    ::UpdateWindow(hWnd_);
    ::RedrawWindow(hWnd_, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

    CreateFonts();
    CreateControls();
    LoadConfigToUi();
    log_.SetEnabled(IsChecked(hChkLog_));

    ::DragAcceptFiles(hWnd_, TRUE);

    SetState(AppState::READY);
    log_.Info(L"Application started.");
    log_.Info(L"Working directory: " + util::ModuleDirectory());
    SetProgress(0);
}

void App::CreateControls()
{
    // ---- ウィンドウ上部（常時表示）----
    hLblFile_    = MakeStatic(hWnd_, L"対象ファイル", IDC_LBL_FILE, hFontUi_);
    hTargetEdit_ = MakeEdit(hWnd_, IDC_EDIT_TARGETFILE, ES_AUTOHSCROLL, hFontUi_);
    hBrowseBtn_  = MakeButton(hWnd_, L"参照...", IDC_BTN_BROWSE, BS_PUSHBUTTON, hFontUi_);

    hLblSwitch_     = MakeStatic(hWnd_, L"スイッチ名", IDC_LBL_SWITCH, hFontUi_);
    hSwitchEdit_    = MakeEdit(hWnd_, IDC_EDIT_SWITCHNAME, ES_AUTOHSCROLL | ES_UPPERCASE, hFontUi_);
    hLblHintSwitch_ = MakeStatic(hWnd_, L"", IDC_LBL_HINT_SWITCH, hFontUi_);

    // ---- タブコントロール ----
    hTab_ = ::CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TCS_TABS | TCS_SINGLELINE,
        0, 0, 10, 10, hWnd_, (HMENU)IDC_TAB_MAIN, hInst_, nullptr);
    ApplyFont(hTab_);

    static const wchar_t* titles[PG_COUNT] = {
        L"基本設定", L"一括処理", L"検索条件", L"旧コード", L"新コード", L"追加コード",
        L"検索結果", L"プレビュー", L"スイッチ管理", L"システム情報", L"履歴", L"ログ"
    };
    for (int i = 0; i < PG_COUNT; ++i) {
        TCITEMW item;
        ::ZeroMemory(&item, sizeof(item));
        item.mask = TCIF_TEXT;
        item.pszText = (LPWSTR)titles[i];
        ::SendMessageW(hTab_, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&item);
    }

    for (int i = 0; i < PG_COUNT; ++i) {
        hPage_[i] = ::CreateWindowExW(0, kPageClass, L"",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 10, 10, hWnd_, nullptr, hInst_, this);
        // ページをタブより手前に置く（タブの再描画でページ上の子が塗りつぶされないように）
        ::SetWindowPos(hPage_[i], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ApplyFont(hPage_[i]);
    }

    CreateBasicPage();
    CreateBatchPage();
    CreateOldCodePage();
    CreateSearchPage();
    CreateNewCodePage();
    CreateAddCodePage();
    CreateResultPage();
    CreatePreviewPage();
    CreateSwitchPage();
    CreateSystemPage();
    CreateHistoryPage();
    CreateLogPage();

    // ---- 下部ボタン ----
    hBtnSearch_  = MakeButton(hWnd_, L"検索",     IDC_BTN_SEARCH,  BS_PUSHBUTTON | BS_DEFPUSHBUTTON, hFontUi_);
    hBtnPreview_ = MakeButton(hWnd_, L"プレビュー", IDC_BTN_PREVIEW, BS_PUSHBUTTON, hFontUi_);
    hBtnDryRun_  = MakeButton(hWnd_, L"ドライラン", IDC_BTN_DRYRUN,  BS_PUSHBUTTON, hFontUi_);
    hBtnExecute_ = MakeButton(hWnd_, L"実行",     IDC_BTN_EXECUTE, BS_PUSHBUTTON, hFontUi_);
    hBtnUndo_    = MakeButton(hWnd_, L"元に戻す", IDC_BTN_UNDO,    BS_PUSHBUTTON, hFontUi_);
    hBtnExit_    = MakeButton(hWnd_, L"終了",     IDC_BTN_EXIT,    BS_PUSHBUTTON, hFontUi_);

    hProgress_ = ::CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        0, 0, 10, 10, hWnd_, (HMENU)IDC_PROGRESS, hInst_, nullptr);
    ::SendMessageW(hProgress_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    hStatus_ = ::CreateWindowExW(0, STATUSCLASSNAMEW, L"",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd_, nullptr, hInst_, nullptr);
    ApplyFont(hStatus_);

    ::ShowWindow(hPage_[PG_BASIC], SW_SHOW);
    ::SendMessageW(hTab_, TCM_SETCURSEL, PG_BASIC, 0);
}

void App::CreateBasicPage()
{
    HWND p = hPage_[PG_BASIC];

    hGrpStyle_ = MakeGroup(p, L"置換方式", IDC_GRP_STYLE, hFontUi_);
    hRadStyle_[0] = MakeRadio(p, L"#ifdef スイッチ／新コード／#else／旧コード／#endif  （方式A: 新コード優先）",
                              IDC_RAD_STYLE_A, WS_GROUP, hFontUi_);
    hRadStyle_[1] = MakeRadio(p, L"#ifndef スイッチ／旧コード／#else／新コード／#endif  （方式B: 旧コード優先）",
                              IDC_RAD_STYLE_B, 0, hFontUi_);
    hRadStyle_[2] = MakeRadio(p, L"コメント付き方式  （方式C: レビュー性重視）",
                              IDC_RAD_STYLE_C, 0, hFontUi_);
    hRadStyle_[3] = MakeRadio(p, L"開始終了コメント方式  （方式D: 後から再検出しやすい）",
                              IDC_RAD_STYLE_D, 0, hFontUi_);

    hLblTemplate_ = MakeStatic(p,
        L"テンプレート方式（${SWITCH} / ${OLD_CODE} / ${NEW_CODE} / ${DATE} / ${USER} / ${FILE}）",
        IDC_LBL_TEMPLATE, hFontUi_);
    hEditTemplate_ = MakeEdit(p, IDC_EDIT_TEMPLATE,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontMono_);

    hGrpOption_ = MakeGroup(p, L"オプション", IDC_GRP_OPTION, hFontUi_);
    hChkBackup_      = MakeCheck(p, L"バックアップを作成する",       IDC_CHK_BACKUP,      hFontUi_);
    hChkReport_      = MakeCheck(p, L"HTML差分レポートを出力する",   IDC_CHK_REPORT,      hFontUi_);
    hChkLog_         = MakeCheck(p, L"実行ログを出力する",           IDC_CHK_LOG,         hFontUi_);
    hChkOpenReport_  = MakeCheck(p, L"実行後にレポートを開く",       IDC_CHK_OPENREPORT,  hFontUi_);
    hChkDryRunOnly_  = MakeCheck(p, L"ドライランのみ（実行を抑止）", IDC_CHK_DRYRUNONLY,  hFontUi_);
    hChkAutoIndent_  = MakeCheck(p, L"インデントを自動調整する",     IDC_CHK_AUTOINDENT,  hFontUi_);
    hChkWarnComment_ = MakeCheck(p, L"コメント内の一致を警告する",   IDC_CHK_WARNCOMMENT, hFontUi_);
    hChkWarnPreproc_ = MakeCheck(p, L"既存コンパイルスイッチ近傍を警告する", IDC_CHK_WARNPREPROC, hFontUi_);

    hGrpAi_ = MakeGroup(p, L"AI 連携（変更仕様書 / 検査仕様書の生成）", IDC_GRP_AI, hFontUi_);
    hLblAiProvider_ = MakeStatic(p, L"API 種別", IDC_LBL_AIPROVIDER, hFontUi_);
    hCmbAiProvider_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_AIPROVIDER, hInst_, nullptr);
    ApplyFont(hCmbAiProvider_);
    ::SendMessageW(hCmbAiProvider_, CB_ADDSTRING, 0, (LPARAM)L"Genspark API（既定）");
    ::SendMessageW(hCmbAiProvider_, CB_ADDSTRING, 0, (LPARAM)L"OpenAI 互換 API（GPT / Azure / Ollama 等）");
    ::SendMessageW(hCmbAiProvider_, CB_SETCURSEL, 0, 0);

    hLblAiKey_      = MakeStatic(p, L"API キー", IDC_LBL_AIKEY, hFontUi_);
    hEditAiKey_     = MakeEdit(p, IDC_EDIT_AIKEY, ES_AUTOHSCROLL | ES_PASSWORD, hFontUi_);
    hBtnAiTest_     = MakeButton(p, L"接続テスト", IDC_BTN_AITEST, BS_PUSHBUTTON, hFontUi_);
    hLblAiEndpoint_ = MakeStatic(p, L"エンドポイント", IDC_LBL_AIENDPOINT, hFontUi_);
    hEditAiEndpoint_= MakeEdit(p, IDC_EDIT_AIENDPOINT, ES_AUTOHSCROLL, hFontUi_);
    hLblAiModel_    = MakeStatic(p, L"モデル", IDC_LBL_AIMODEL, hFontUi_);
    hEditAiModel_   = MakeEdit(p, IDC_EDIT_AIMODEL, ES_AUTOHSCROLL, hFontUi_);
    hLblAiTimeout_  = MakeStatic(p, L"タイムアウト(秒)", IDC_LBL_AITIMEOUT, hFontUi_);
    hEditAiTimeout_ = MakeEdit(p, IDC_EDIT_AITIMEOUT, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);

    hGrpHit_ = MakeGroup(p, L"ヒット時の動作", IDC_GRP_HIT, hFontUi_);
    hRadHit_[0] = MakeRadio(p, L"1件の場合のみ自動選択",     IDC_RAD_HIT_AUTO,   WS_GROUP, hFontUi_);
    hRadHit_[1] = MakeRadio(p, L"最初の1件を対象にする",     IDC_RAD_HIT_FIRST,  0, hFontUi_);
    hRadHit_[2] = MakeRadio(p, L"すべての一致を対象にする", IDC_RAD_HIT_ALL,    0, hFontUi_);
    hRadHit_[3] = MakeRadio(p, L"ユーザーが一覧から選択する", IDC_RAD_HIT_MANUAL, 0, hFontUi_);

    // ---- 追加機能: ログ出力 / レポート作成単位 / GoogleTest ----
    hGrpOutCfg_ = MakeGroup(p, L"ログ / レポート / テスト", IDC_GRP_OUTCONF, hFontUi_);
    hChkLogEnabled_ = MakeCheck(p, L"ログ出力を有効にする（INPUT/OUTPUT_LOG を挿入）",
                                IDC_CHK_LOGENABLED, hFontUi_);
    hChkLogTemp_ = MakeCheck(p, L"TEMP_LOG を差し替え箇所に挿入する",
                             IDC_CHK_LOGTEMP, hFontUi_);
    hChkLogRet_ = MakeCheck(p, L"戻り値も記録する（整数型のみ）",
                            IDC_CHK_LOGRET, hFontUi_);
    hLblReportScope_ = MakeStatic(p, L"レポート作成単位", IDC_LBL_REPORTSCOPE, hFontUi_);
    hCmbReportScope_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_REPORTSCOPE, hInst_, nullptr);
    ApplyFont(hCmbReportScope_);
    ::SendMessageW(hCmbReportScope_, CB_ADDSTRING, 0, (LPARAM)L"変更単位（従来）");
    ::SendMessageW(hCmbReportScope_, CB_ADDSTRING, 0, (LPARAM)L"ファイル単位");
    ::SendMessageW(hCmbReportScope_, CB_ADDSTRING, 0, (LPARAM)L"プロジェクト単位");
    ::SendMessageW(hCmbReportScope_, CB_SETCURSEL, 0, 0);
    hLblLoadIter_ = MakeStatic(p, L"負荷試験 反復", IDC_LBL_LOADITER, hFontUi_);
    hEditLoadIter_ = MakeEdit(p, IDC_EDIT_LOADITER, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);
    hChkGTest_ = MakeCheck(p, L"GoogleTest を生成する", IDC_CHK_GTEST, hFontUi_);
    hBtnLogHeader_ = MakeButton(p, L"ログ定義を出力", IDC_BTN_LOGHEADER, BS_PUSHBUTTON, hFontUi_);
    hBtnGTestGen_ = MakeButton(p, L"テスト生成", IDC_BTN_GTESTGEN, BS_PUSHBUTTON, hFontUi_);
    hBtnGTestRun_ = MakeButton(p, L"テスト実行", IDC_BTN_GTESTRUN, BS_PUSHBUTTON, hFontUi_);

    // ---- 追加機能: ログ実装の拡張子 / 負荷試験トグル ----
    hLblLogExt_ = MakeStatic(p, L"ログ実装の拡張子", IDC_LBL_LOGEXT, hFontUi_);
    hCmbLogExt_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_LOGEXT, hInst_, nullptr);
    ApplyFont(hCmbLogExt_);
    ::SendMessageW(hCmbLogExt_, CB_ADDSTRING, 0, (LPARAM)L".c（C として出力）");
    ::SendMessageW(hCmbLogExt_, CB_ADDSTRING, 0, (LPARAM)L".cpp（C++ として出力）");
    ::SendMessageW(hCmbLogExt_, CB_SETCURSEL, 0, 0);
    hChkLoadTest_ = MakeCheck(p, L"負荷試験を行う", IDC_CHK_LOADTEST, hFontUi_);

    // GoogleTest の対象範囲（実行履歴の組み合わせ）
    hLblGtRange_ = MakeStatic(p, L"テスト対象範囲", IDC_LBL_GTRANGE, hFontUi_);
    hCmbGtRangeFrom_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_GTRANGEFROM, hInst_, nullptr);
    ApplyFont(hCmbGtRangeFrom_);
    hCmbGtRangeTo_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_GTRANGETO, hInst_, nullptr);
    ApplyFont(hCmbGtRangeTo_);
}

// -----------------------------------------------------------------------------
// 17.2.1 追加UI（一括処理タブ）
// -----------------------------------------------------------------------------
void App::CreateBatchPage()
{
    HWND p = hPage_[PG_BATCH];

    hGrpTargetKind_ = MakeGroup(p, L"対象種別", IDC_GRP_TARGETKIND, hFontUi_);
    hRadTargetKind_[0] = MakeRadio(p, L"単一ファイル", IDC_RAD_TARGET_FILE,   WS_GROUP, hFontUi_);
    hRadTargetKind_[1] = MakeRadio(p, L"フォルダ",     IDC_RAD_TARGET_FOLDER, 0, hFontUi_);

    hLblFolder_       = MakeStatic(p, L"対象フォルダ", IDC_LBL_FOLDER, hFontUi_);
    hFolderEdit_      = MakeEdit(p, IDC_EDIT_FOLDER, ES_AUTOHSCROLL, hFontUi_);
    hFolderBrowseBtn_ = MakeButton(p, L"参照...", IDC_BTN_FOLDERBROWSE, BS_PUSHBUTTON, hFontUi_);

    hGrpExtFilter_ = MakeGroup(p, L"拡張子フィルタ", IDC_GRP_EXTFILTER, hFontUi_);
    std::vector<std::wstring> cand = FolderScanner::CandidateExtensions();
    std::vector<std::wstring> defs = FolderScanner::DefaultExtensions();
    for (size_t i = 0; i < cand.size() && i < 6; ++i) {
        bool on = false;
        for (size_t k = 0; k < defs.size(); ++k) if (defs[k] == cand[i]) on = true;
        hChkExt_[i] = MakeCheck(p, cand[i].c_str(), IDC_CHK_EXT_BASE + (int)i, hFontUi_);
        SetChecked(hChkExt_[i], on);
    }
    hChkSubdirs_ = MakeCheck(p, L"サブフォルダを含める", IDC_CHK_SUBDIRS, hFontUi_);
    SetChecked(hChkSubdirs_, true);

    hBtnScan_        = MakeButton(p, L"走査", IDC_BTN_SCAN, BS_PUSHBUTTON, hFontUi_);
    hBtnFileSelAll_  = MakeButton(p, L"すべて選択", IDC_BTN_FILESELALL, BS_PUSHBUTTON, hFontUi_);
    hBtnFileSelNone_ = MakeButton(p, L"すべて解除", IDC_BTN_FILESELNONE, BS_PUSHBUTTON, hFontUi_);
    hLblScanInfo_    = MakeStatic(p, L"", IDC_LBL_SCANINFO, hFontUi_);

    hListFiles_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, p, (HMENU)IDC_LIST_FILES, hInst_, nullptr);
    ApplyFont(hListFiles_);
    ::SendMessageW(hListFiles_, LVM_SETEXTENDEDLISTVIEWSTYLE,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);

    struct Col { const wchar_t* name; int width; int fmt; };
    static const Col cols[] = {
        { L"No",        44,  LVCFMT_RIGHT  },
        { L"ファイル名",260, LVCFMT_LEFT   },
        { L"フォルダ",  300, LVCFMT_LEFT   },
        { L"ヒット",    60,  LVCFMT_RIGHT  },
        { L"置換対象",  76,  LVCFMT_RIGHT  },
        { L"状態",      110, LVCFMT_LEFT   },
    };
    for (int i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); ++i) {
        LVCOLUMNW c;
        ::ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        c.pszText = (LPWSTR)cols[i].name;
        c.cx = cols[i].width;
        c.iSubItem = i;
        c.fmt = cols[i].fmt;
        ::SendMessageW(hListFiles_, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
}

void App::CreateOldCodePage()
{
    HWND p = hPage_[PG_OLD];
    hOldEdit_ = MakeEdit(p, IDC_EDIT_OLDCODE,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN, hFontMono_);
    hLblOldInfo_ = MakeStatic(p, L"", IDC_LBL_OLDINFO, hFontUi_);
}

void App::CreateNewCodePage()
{
    HWND p = hPage_[PG_NEW];
    hNewEdit_ = MakeEdit(p, IDC_EDIT_NEWCODE,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN, hFontMono_);
    hLblNewInfo_ = MakeStatic(p, L"", IDC_LBL_NEWINFO, hFontUi_);
}

void App::CreateResultPage()
{
    HWND p = hPage_[PG_RESULT];

    hBtnSelectAll_ = MakeButton(p, L"すべて選択",   IDC_BTN_SELECTALL, BS_PUSHBUTTON, hFontUi_);
    hBtnClearSel_  = MakeButton(p, L"選択を解除",   IDC_BTN_CLEARSEL,  BS_PUSHBUTTON, hFontUi_);
    hBtnExportCsv_ = MakeButton(p, L"CSV出力",      IDC_BTN_EXPORTCSV,  BS_PUSHBUTTON, hFontUi_);
    hBtnExportMd_  = MakeButton(p, L"Markdown出力", IDC_BTN_EXPORTMD,   BS_PUSHBUTTON, hFontUi_);
    hBtnExportJson_= MakeButton(p, L"JSON出力",     IDC_BTN_EXPORTJSON, BS_PUSHBUTTON, hFontUi_);
    hLblResultInfo_ = MakeStatic(p, L"", IDC_LBL_RESULTINFO, hFontUi_);

    hListView_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 10, 10, p, (HMENU)IDC_LIST_MATCHES, hInst_, nullptr);
    ApplyFont(hListView_);
    ::SendMessageW(hListView_, LVM_SETEXTENDEDLISTVIEWSTYLE,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_CHECKBOXES);

    struct Col { const wchar_t* name; int width; int fmt; };
    static const Col cols[] = {
        { L"No",        44,  LVCFMT_RIGHT  },
        { L"選択",      48,  LVCFMT_CENTER },
        { L"ファイル名",190, LVCFMT_LEFT   },
        { L"行番号",    62,  LVCFMT_RIGHT  },
        { L"列番号",    62,  LVCFMT_RIGHT  },
        { L"一致種別",  82,  LVCFMT_LEFT   },
        { L"一致率",    58,  LVCFMT_RIGHT  },
        { L"状態",      64,  LVCFMT_LEFT   },
        { L"プレビュー",330, LVCFMT_LEFT   },
    };
    for (int i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); ++i) {
        LVCOLUMNW c;
        ::ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        c.pszText = (LPWSTR)cols[i].name;
        c.cx = cols[i].width;
        c.iSubItem = i;
        c.fmt = cols[i].fmt;
        ::SendMessageW(hListView_, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
}

void App::CreatePreviewPage()
{
    HWND p = hPage_[PG_PREVIEW];

    hCmbPreviewMode_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_PREVIEWMODE, hInst_, nullptr);
    ApplyFont(hCmbPreviewMode_);
    static const wchar_t* modes[4] = {
        L"前後比較表示", L"単純表示", L"左右比較表示", L"HTML表示（ブラウザで開く）"
    };
    for (int i = 0; i < 4; ++i)
        ::SendMessageW(hCmbPreviewMode_, CB_ADDSTRING, 0, (LPARAM)modes[i]);
    ::SendMessageW(hCmbPreviewMode_, CB_SETCURSEL, 0, 0);

    hBtnOpenHtml_ = MakeButton(p, L"HTMLレポートを開く", IDC_BTN_OPENHTML, BS_PUSHBUTTON, hFontUi_);
    hBtnWinMerge_ = MakeButton(p, L"WinMerge で比較", IDC_BTN_WINMERGE, BS_PUSHBUTTON, hFontUi_);

    hPreviewEdit_ = MakeEdit(p, IDC_EDIT_PREVIEW,
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL, hFontMono_);

    // 比較する範囲（実行履歴の組み合わせ）
    hLblPairFrom_ = MakeStatic(p, L"比較範囲", IDC_LBL_PAIRFROM, hFontUi_);
    hCmbPairFrom_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_PAIRFROM, hInst_, nullptr);
    ApplyFont(hCmbPairFrom_);
    hLblPairTo_ = MakeStatic(p, L"～", IDC_LBL_PAIRTO, hFontUi_);
    hCmbPairTo_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_PAIRTO, hInst_, nullptr);
    ApplyFont(hCmbPairTo_);
}


// -----------------------------------------------------------------------------
// スイッチ管理タブ（スイッチ定義ヘッダの一元管理）
// -----------------------------------------------------------------------------
void App::CreateSwitchPage()
{
    HWND p = hPage_[PG_SWITCH];

    hLblSwHeader_        = MakeStatic(p, L"スイッチ定義ヘッダ", IDC_LBL_SWHEADER, hFontUi_);
    hSwHeaderEdit_       = MakeEdit(p, IDC_EDIT_SWHEADER, ES_AUTOHSCROLL | ES_READONLY, hFontUi_);
    hBtnSwHeaderBrowse_  = MakeButton(p, L"参照...", IDC_BTN_SWHEADERBROWSE, BS_PUSHBUTTON, hFontUi_);
    hBtnSwHeaderLoad_    = MakeButton(p, L"読込",    IDC_BTN_SWHEADERLOAD,   BS_PUSHBUTTON, hFontUi_);

    hGrpSwAuto_ = MakeGroup(p, L"自動化オプション（実行時に適用）", IDC_GRP_SWAUTO, hFontUi_);
    hChkManageSwHeader_ = MakeCheck(p,
        L"実行時にスイッチ定義ヘッダを生成／更新する（スイッチの定義を 1 ファイルへ集約）",
        IDC_CHK_MANAGESWHEADER, hFontUi_);
    hChkAddInclude_ = MakeCheck(p,
        L"対象ファイルへ #include を追加する（重複追加はしません）",
        IDC_CHK_ADDINCLUDE, hFontUi_);

    hLblSwHeaderName_ = MakeStatic(p, L"ヘッダファイル名", IDC_LBL_SWHEADERNAME, hFontUi_);
    hEditSwHeaderName_ = MakeEdit(p, IDC_EDIT_SWHEADERNAME, ES_AUTOHSCROLL, hFontUi_);
    hLblSwHeaderDir_ = MakeStatic(p, L"出力先フォルダ", IDC_LBL_SWHEADERDIR, hFontUi_);
    hEditSwHeaderDir_ = MakeEdit(p, IDC_EDIT_SWHEADERDIR, ES_AUTOHSCROLL, hFontUi_);
    hBtnSwHeaderDir_ = MakeButton(p, L"参照...", IDC_BTN_SWHEADERDIR, BS_PUSHBUTTON, hFontUi_);
    hLblSwMemo_ = MakeStatic(p, L"スイッチの説明", IDC_LBL_SWMEMO, hFontUi_);
    hEditSwMemo_ = MakeEdit(p, IDC_EDIT_SWMEMO, ES_AUTOHSCROLL, hFontUi_);

    hBtnSwCreate_  = MakeButton(p, L"ヘッダを今すぐ生成/更新", IDC_BTN_SWCREATE, BS_PUSHBUTTON, hFontUi_);
    hBtnSwEnable_  = MakeButton(p, L"有効化",     IDC_BTN_SWENABLE,  BS_PUSHBUTTON, hFontUi_);
    hBtnSwDisable_ = MakeButton(p, L"無効化",     IDC_BTN_SWDISABLE, BS_PUSHBUTTON, hFontUi_);
    hBtnSwRemove_  = MakeButton(p, L"選択を削除", IDC_BTN_SWREMOVE,  BS_PUSHBUTTON, hFontUi_);
    hBtnSwOpen_    = MakeButton(p, L"ヘッダを開く", IDC_BTN_SWOPEN,  BS_PUSHBUTTON, hFontUi_);
    hLblSwInfo_    = MakeStatic(p, L"", IDC_LBL_SWINFO, hFontUi_);

    hListSwitches_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        0, 0, 10, 10, p, (HMENU)IDC_LIST_SWITCHES, hInst_, nullptr);
    ApplyFont(hListSwitches_);
    ::SendMessageW(hListSwitches_, LVM_SETEXTENDEDLISTVIEWSTYLE,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    struct Col { const wchar_t* name; int width; int fmt; };
    static const Col cols[] = {
        { L"No",        44,  LVCFMT_RIGHT },
        { L"スイッチ名",   190, LVCFMT_LEFT  },
        { L"状態",      90,  LVCFMT_LEFT  },
        { L"登録元",    150, LVCFMT_LEFT  },
        { L"説明 / 更新", 420, LVCFMT_LEFT  },
    };
    for (int i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); ++i) {
        LVCOLUMNW c;
        ::ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        c.pszText = (LPWSTR)cols[i].name;
        c.cx = cols[i].width;
        c.iSubItem = i;
        c.fmt = cols[i].fmt;
        ::SendMessageW(hListSwitches_, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
}


// =============================================================================
// 検索条件タブ（後続版: 類似一致 / 指定行範囲 / 関数単位 / 出力形式 / WinMerge / Undo）
// =============================================================================
void App::CreateSearchPage()
{
    HWND p = hPage_[PG_SEARCH];

    hGrpSimilar_ = MakeGroup(p, L"類似一致検索（空白の揺れを許容）", IDC_GRP_SIMILAR, hFontUi_);
    hChkSimilar_ = MakeCheck(p, L"類似一致検索を有効にする（置換対象へは自動選択されません）",
                             IDC_CHK_SIMILAR, hFontUi_);
    hLblSimThreshold_ = MakeStatic(p, L"一致率のしきい値(%)", IDC_LBL_SIMTHRESHOLD, hFontUi_);
    hEditSimThreshold_ = MakeEdit(p, IDC_EDIT_SIMTHRESHOLD, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);

    hGrpLineRange_ = MakeGroup(p, L"指定行範囲検索", IDC_GRP_LINERANGE, hFontUi_);
    hChkLineRange_ = MakeCheck(p, L"指定した行範囲だけを検索する", IDC_CHK_LINERANGE, hFontUi_);
    hLblLineFrom_  = MakeStatic(p, L"開始行", IDC_LBL_LINEFROM, hFontUi_);
    hEditLineFrom_ = MakeEdit(p, IDC_EDIT_LINEFROM, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);
    hLblLineTo_    = MakeStatic(p, L"終了行", IDC_LBL_LINETO, hFontUi_);
    hEditLineTo_   = MakeEdit(p, IDC_EDIT_LINETO, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);

    hGrpFuncScope_ = MakeGroup(p, L"関数単位検索", IDC_GRP_FUNCSCOPE, hFontUi_);
    hChkFuncScope_ = MakeCheck(p, L"指定した関数の本体（{ } の中）だけを検索する",
                               IDC_CHK_FUNCSCOPE, hFontUi_);
    hLblFuncName_  = MakeStatic(p, L"関数名", IDC_LBL_FUNCNAME, hFontUi_);
    hEditFuncName_ = MakeEdit(p, IDC_EDIT_FUNCNAME, ES_AUTOHSCROLL, hFontUi_);

    hGrpOutput_ = MakeGroup(p, L"出力形式", IDC_GRP_OUTPUT, hFontUi_);
    hChkReportMd_   = MakeCheck(p, L"Markdown レポートも出力する (.md)", IDC_CHK_REPORTMD, hFontUi_);
    hChkReportJson_ = MakeCheck(p, L"JSON レポートも出力する (.json)", IDC_CHK_REPORTJSON, hFontUi_);
    hLblEncoding_   = MakeStatic(p, L"文字コード", IDC_LBL_ENCODING, hFontUi_);
    hCmbEncoding_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_ENCODING, hInst_, nullptr);
    ApplyFont(hCmbEncoding_);
    static const wchar_t* encs[5] = {
        L"自動判定", L"UTF-8", L"UTF-8 (BOM)", L"Shift_JIS", L"EUC-JP"
    };
    for (int i = 0; i < 5; ++i)
        ::SendMessageW(hCmbEncoding_, CB_ADDSTRING, 0, (LPARAM)encs[i]);
    ::SendMessageW(hCmbEncoding_, CB_SETCURSEL, 0, 0);

    hGrpWinMerge_ = MakeGroup(p, L"WinMerge 連携", IDC_GRP_WINMERGE, hFontUi_);
    hLblWinMerge_ = MakeStatic(p, L"WinMergeU.exe のパス", IDC_LBL_WINMERGE, hFontUi_);
    hEditWinMerge_ = MakeEdit(p, IDC_EDIT_WINMERGE, ES_AUTOHSCROLL, hFontUi_);
    hBtnWinMergeBrowse_ = MakeButton(p, L"参照...", IDC_BTN_WINMERGEBROWSE, BS_PUSHBUTTON, hFontUi_);

    hGrpUndoOpt_ = MakeGroup(p, L"Undo（元に戻す）", IDC_GRP_UNDOOPT, hFontUi_);
    hLblUndoMax_ = MakeStatic(p, L"保持する操作世代数", IDC_LBL_UNDOMAX, hFontUi_);
    hEditUndoMax_ = MakeEdit(p, IDC_EDIT_UNDOMAX, ES_AUTOHSCROLL | ES_NUMBER, hFontUi_);

    hLblSearchInfo_ = MakeStatic(p, L"", IDC_LBL_SEARCHINFO, hFontUi_);
}

void App::CreateLogPage()
{
    HWND p = hPage_[PG_LOG];
    hBtnClearLog_ = MakeButton(p, L"ログをクリア", IDC_BTN_CLEARLOG, BS_PUSHBUTTON, hFontUi_);
    hLogEdit_ = MakeEdit(p, IDC_EDIT_LOG,
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL, hFontMono_);
}

// -----------------------------------------------------------------------------
// システム情報タブ（変更仕様書 / 検査仕様書の生成）
// -----------------------------------------------------------------------------
void App::CreateSystemPage()
{
    HWND p = hPage_[PG_SYSTEM];

    hLblSysName_     = MakeStatic(p, L"システム名", IDC_LBL_SYSNAME, hFontUi_);
    hEditSysName_    = MakeEdit(p, IDC_EDIT_SYSNAME, ES_AUTOHSCROLL, hFontUi_);
    hLblSysOverview_ = MakeStatic(p, L"システム概要", IDC_LBL_SYSOVERVIEW, hFontUi_);
    hEditSysOverview_= MakeEdit(p, IDC_EDIT_SYSOVERVIEW,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontUi_);
    hLblSysPurpose_  = MakeStatic(p, L"目的", IDC_LBL_SYSPURPOSE, hFontUi_);
    hEditSysPurpose_ = MakeEdit(p, IDC_EDIT_SYSPURPOSE,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontUi_);
    hLblSysMod_      = MakeStatic(p, L"修正箇所概要", IDC_LBL_SYSMOD, hFontUi_);
    hEditSysMod_     = MakeEdit(p, IDC_EDIT_SYSMOD,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontUi_);
    hLblSysExtra_    = MakeStatic(p, L"追加指示（任意）", IDC_LBL_SYSEXTRA, hFontUi_);
    hEditSysExtra_   = MakeEdit(p, IDC_EDIT_SYSEXTRA,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontUi_);

    hBtnSpecChange_  = MakeButton(p, L"変更仕様書を生成", IDC_BTN_SPECCHANGE, BS_PUSHBUTTON, hFontUi_);
    hBtnSpecInspect_ = MakeButton(p, L"検査仕様書を生成", IDC_BTN_SPECINSPECT, BS_PUSHBUTTON, hFontUi_);
    hBtnSpecBoth_    = MakeButton(p, L"両方を生成", IDC_BTN_SPECBOTH, BS_PUSHBUTTON, hFontUi_);
    hChkAiOpenReport_= MakeCheck(p, L"生成後にレポートを開く", IDC_CHK_AIOPENREPORT, hFontUi_);
    hBtnSpecOpen_    = MakeButton(p, L"レポートを開く", IDC_BTN_SPECOPEN, BS_PUSHBUTTON, hFontUi_);
    hLblSpecInfo_    = MakeStatic(p, L"", IDC_LBL_SPECINFO, hFontUi_);

    // AI 連携が使えない環境向け: 依頼文を txt で出力する
    hBtnPromptChange_  = MakeButton(p, L"プロンプトtxt（変更仕様書）", IDC_BTN_PROMPTCHANGE,  BS_PUSHBUTTON, hFontUi_);
    hBtnPromptInspect_ = MakeButton(p, L"プロンプトtxt（検査仕様書）", IDC_BTN_PROMPTINSPECT, BS_PUSHBUTTON, hFontUi_);
    hBtnPromptBoth_    = MakeButton(p, L"両方のプロンプトtxt",        IDC_BTN_PROMPTBOTH,    BS_PUSHBUTTON, hFontUi_);

    // AI チャットの回答を貼り付けてレポート化する
    hBtnMakeChange_  = MakeButton(p, L"この内容を変更仕様書にする", IDC_BTN_MAKECHANGE,  BS_PUSHBUTTON, hFontUi_);
    hBtnMakeInspect_ = MakeButton(p, L"この内容を検査仕様書にする", IDC_BTN_MAKEINSPECT, BS_PUSHBUTTON, hFontUi_);
    hLblPasteNote_   = MakeStatic(p,
        L"（API を使わない場合は、出力した txt を AI チャットへ貼り、返ってきた回答を下の欄へ貼り付けてボタンを押してください）",
        IDC_LBL_PASTENOTE, hFontUi_);

    hLblSpecOutNote_ = MakeStatic(p,
        L"出力（AI の生成結果 / AI チャットの回答をここに貼り付け）",
        IDC_LBL_SPECOUTNOTE, hFontUi_);
    hEditSpecOut_ = MakeEdit(p, IDC_EDIT_SPECOUT,
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN, hFontMono_);

    // AI に渡す変更範囲（実行履歴の組み合わせ）
    hLblSysPairFrom_ = MakeStatic(p, L"変更範囲(前)", IDC_LBL_SYSPAIRFROM, hFontUi_);
    hCmbSysPairFrom_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_SYSPAIRFROM, hInst_, nullptr);
    ApplyFont(hCmbSysPairFrom_);
    hLblSysPairTo_ = MakeStatic(p, L"～", IDC_LBL_SYSPAIRTO, hFontUi_);
    hCmbSysPairTo_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 220, p, (HMENU)IDC_CMB_SYSPAIRTO, hInst_, nullptr);
    ApplyFont(hCmbSysPairTo_);
}

// -----------------------------------------------------------------------------
// レイアウト
// -----------------------------------------------------------------------------
void App::LayoutPage(int index, const RECT& rc)
{
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    if (W < 50 || H < 50) return;
    const int M = 8;

    switch (index) {
    case PG_BASIC: {
        // 2 カラム配置: 左=置換方式 / オプション / ヒット時の動作、右=ログ・レポート・テスト / AI 連携
        const int M = 10;
        int gw = W - M * 2;
        int gap = 12;
        int colW = (gw - gap) / 2;
        int lx = M, rx2 = M + colW + gap;

        int y = M;
        // ---- 左 1: 置換方式 ----
        int gh = 178;
        ::MoveWindow(hGrpStyle_, lx, y, colW, gh, TRUE);
        int ix = lx + 12, iw = colW - 24;
        ::MoveWindow(hRadStyle_[0], ix, y + 22, iw, 18, TRUE);
        ::MoveWindow(hRadStyle_[1], ix, y + 44, iw, 18, TRUE);
        ::MoveWindow(hRadStyle_[2], ix, y + 66, iw, 18, TRUE);
        ::MoveWindow(hRadStyle_[3], ix, y + 88, iw, 18, TRUE);
        ::MoveWindow(hLblTemplate_, ix, y + 110, iw, 16, TRUE);
        ::MoveWindow(hEditTemplate_, ix, y + 126, iw, 42, TRUE);
        y += gh + 10;

        // ---- 左 2: オプション ----
        int oh = 116;
        ::MoveWindow(hGrpOption_, lx, y, colW, oh, TRUE);
        int cw = (colW - 24) / 2;
        ::MoveWindow(hChkBackup_,      ix,          y + 22, cw, 18, TRUE);
        ::MoveWindow(hChkReport_,      ix,          y + 44, cw, 18, TRUE);
        ::MoveWindow(hChkLog_,         ix,          y + 66, cw, 18, TRUE);
        ::MoveWindow(hChkOpenReport_,  ix,          y + 88, cw, 18, TRUE);
        ::MoveWindow(hChkDryRunOnly_,  ix + cw,     y + 22, cw, 18, TRUE);
        ::MoveWindow(hChkAutoIndent_,  ix + cw,     y + 44, cw, 18, TRUE);
        ::MoveWindow(hChkWarnComment_, ix + cw,     y + 66, cw, 18, TRUE);
        ::MoveWindow(hChkWarnPreproc_, ix + cw,     y + 88, cw, 18, TRUE);
        y += oh + 10;

        // ---- 左 3: ヒット時の動作（残り高さ）----
        int hh = H - M - y;
        if (hh < 96) hh = 96;
        ::MoveWindow(hGrpHit_, lx, y, colW, hh, TRUE);
        ::MoveWindow(hRadHit_[0], ix, y + 22, iw, 18, TRUE);
        ::MoveWindow(hRadHit_[1], ix, y + 44, iw, 18, TRUE);
        ::MoveWindow(hRadHit_[2], ix, y + 66, iw, 18, TRUE);
        ::MoveWindow(hRadHit_[3], ix, y + 88, iw, 18, TRUE);

        // ---- 右 1: ログ / レポート / テスト（上から操作順）----
        int ry = M;
        int och = 236;
        ::MoveWindow(hGrpOutCfg_, rx2, ry, colW, och, TRUE);
        int jx = rx2 + 12, jw = colW - 24;
        ::MoveWindow(hChkLogEnabled_, jx,     ry + 22, jw - 90, 18, TRUE);
        ::MoveWindow(hChkLogRet_,     jx,     ry + 44, jw, 18, TRUE);
        ::MoveWindow(hChkLogTemp_,    jx,     ry + 66, jw, 18, TRUE);
        ::MoveWindow(hLblLogExt_,     jx,          ry + 96, 110, 18, TRUE);
        ::MoveWindow(hCmbLogExt_,     jx + 112,    ry + 92, 110, 200, TRUE);
        ::MoveWindow(hBtnLogHeader_,  jx + 230,    ry + 90, 130, 24, TRUE);
        ::MoveWindow(hLblReportScope_, jx,         ry + 126, 110, 18, TRUE);
        ::MoveWindow(hCmbReportScope_, jx + 112,   ry + 122, jw - 112, 200, TRUE);
        ::MoveWindow(hLblGtRange_,     jx,         ry + 156, 110, 18, TRUE);
        ::MoveWindow(hCmbGtRangeFrom_, jx + 112,   ry + 152, (jw - 112 - 8) / 2, 200, TRUE);
        ::MoveWindow(hCmbGtRangeTo_,   jx + 112 + (jw - 112 - 8) / 2 + 8,
                     ry + 152, (jw - 112 - 8) / 2, 200, TRUE);
        ::MoveWindow(hChkGTest_,     jx,        ry + 182, 150, 18, TRUE);
        ::MoveWindow(hChkLoadTest_,  jx + 156,  ry + 182, 110, 18, TRUE);
        ::MoveWindow(hLblLoadIter_,  jx + 270,  ry + 182, 70, 18, TRUE);
        ::MoveWindow(hEditLoadIter_, jx + 340,  ry + 178, 60, 22, TRUE);
        ::MoveWindow(hBtnGTestGen_,  jx,        ry + 208, 130, 24, TRUE);
        ::MoveWindow(hBtnGTestRun_,  jx + 138,  ry + 208, 130, 24, TRUE);
        ry += och + 10;

        // ---- 右 2: AI 連携 ----
        int aih = 132;
        ::MoveWindow(hGrpAi_, rx2, ry, colW, aih, TRUE);
        ::MoveWindow(hLblAiProvider_, jx, ry + 22, 84, 18, TRUE);
        const int aiBtnW = 96;
        const int aiGap  = 8;
        int aiComboW = jw - 84 - aiGap - aiBtnW;
        if (aiComboW < 120) aiComboW = 120;
        ::MoveWindow(hCmbAiProvider_, jx + 84, ry + 18, aiComboW, 200, TRUE);
        ::MoveWindow(hBtnAiTest_, jx + 84 + aiComboW + aiGap, ry + 18, aiBtnW, 22, TRUE);
        ::MoveWindow(hLblAiKey_, jx, ry + 50, 84, 18, TRUE);
        ::MoveWindow(hEditAiKey_, jx + 84, ry + 46, jw - 84, 22, TRUE);
        ::MoveWindow(hLblAiEndpoint_, jx, ry + 78, 84, 18, TRUE);
        ::MoveWindow(hEditAiEndpoint_, jx + 84, ry + 74, jw - 84, 22, TRUE);
        ::MoveWindow(hLblAiModel_, jx, ry + 106, 84, 18, TRUE);
        int mw2 = (jw - 84) / 2 - 60;
        if (mw2 < 80) mw2 = 80;
        ::MoveWindow(hEditAiModel_, jx + 84, ry + 102, mw2, 22, TRUE);
        ::MoveWindow(hLblAiTimeout_, jx + 84 + mw2 + 8, ry + 106, 96, 18, TRUE);
        ::MoveWindow(hEditAiTimeout_, jx + 84 + mw2 + 106, ry + 102, 60, 22, TRUE);
        break;
    }
    case PG_BATCH: {   // 17.2.1 追加UI
        int gw = W - M * 2;
        int y = M;

        ::MoveWindow(hGrpTargetKind_, M, y, gw, 62, TRUE);
        ::MoveWindow(hRadTargetKind_[0], M + 14, y + 22, 170, 18, TRUE);
        ::MoveWindow(hRadTargetKind_[1], M + 194, y + 22, 170, 18, TRUE);
        y += 62 + 8;

        ::MoveWindow(hLblFolder_, M, y + 4, 92, 18, TRUE);
        int fw = gw - 92 - 74 - 12;
        if (fw < 120) fw = 120;
        ::MoveWindow(hFolderEdit_, M + 92, y, fw, 24, TRUE);
        ::MoveWindow(hFolderBrowseBtn_, M + 92 + fw + 6, y, 74, 24, TRUE);
        y += 24 + 8;

        int eh = 92;
        ::MoveWindow(hGrpExtFilter_, M, y, gw, eh, TRUE);
        int cw = (gw - 28) / 3;
        if (cw < 60) cw = 60;
        for (int i = 0; i < 6; ++i) {
            int row = i / 3, col = i % 3;
            ::MoveWindow(hChkExt_[i], M + 14 + col * cw, y + 22 + row * 22, cw, 18, TRUE);
        }
        ::MoveWindow(hChkSubdirs_, M + 14, y + 22 + 2 * 22, 260, 18, TRUE);
        y += eh + 8;

        ::MoveWindow(hBtnScan_, M, y, 90, 26, TRUE);
        ::MoveWindow(hBtnFileSelAll_, M + 96, y, 110, 26, TRUE);
        ::MoveWindow(hBtnFileSelNone_, M + 212, y, 110, 26, TRUE);
        int ix = M + 330;
        ::MoveWindow(hLblScanInfo_, ix, y + 5, gw - 330, 18, TRUE);
        y += 26 + 6;

        int lh = H - y - M;
        if (lh < 60) lh = 60;
        ::MoveWindow(hListFiles_, M, y, gw, lh, TRUE);
        break;
    }
    case PG_OLD:
    case PG_NEW: {
        int infoH = 20;
        HWND edit = (index == PG_OLD) ? hOldEdit_ : hNewEdit_;
        HWND info = (index == PG_OLD) ? hLblOldInfo_ : hLblNewInfo_;
        ::MoveWindow(edit, M, M, W - M * 2, H - M * 2 - infoH - 6, TRUE);
        ::MoveWindow(info, M, H - M - infoH, W - M * 2, infoH, TRUE);
        break;
    }
    case PG_SEARCH: {
        int gw = W - M * 2;
        int rx = M + 14;
        int rw = gw - 28;
        int y = M;
        const int GH = 78;

        ::MoveWindow(hGrpSimilar_, M, y, gw, GH, TRUE);
        ::MoveWindow(hChkSimilar_, rx, y + 22, rw, 18, TRUE);
        ::MoveWindow(hLblSimThreshold_, rx, y + 50, 150, 18, TRUE);
        ::MoveWindow(hEditSimThreshold_, rx + 152, y + 46, 60, 22, TRUE);
        y += GH + 8;

        ::MoveWindow(hGrpLineRange_, M, y, gw, GH, TRUE);
        ::MoveWindow(hChkLineRange_, rx, y + 22, rw, 18, TRUE);
        ::MoveWindow(hLblLineFrom_, rx, y + 50, 46, 18, TRUE);
        ::MoveWindow(hEditLineFrom_, rx + 48, y + 46, 60, 22, TRUE);
        ::MoveWindow(hLblLineTo_, rx + 120, y + 50, 46, 18, TRUE);
        ::MoveWindow(hEditLineTo_, rx + 168, y + 46, 60, 22, TRUE);
        y += GH + 8;

        ::MoveWindow(hGrpFuncScope_, M, y, gw, GH, TRUE);
        ::MoveWindow(hChkFuncScope_, rx, y + 22, rw, 18, TRUE);
        ::MoveWindow(hLblFuncName_, rx, y + 50, 70, 18, TRUE);
        ::MoveWindow(hEditFuncName_, rx + 72, y + 46, 240, 22, TRUE);
        y += GH + 8;

        ::MoveWindow(hGrpOutput_, M, y, gw, GH, TRUE);
        int cw2 = (gw - 28) / 2;
        ::MoveWindow(hChkReportMd_, rx, y + 22, cw2, 18, TRUE);
        ::MoveWindow(hChkReportJson_, rx + cw2, y + 22, cw2, 18, TRUE);
        ::MoveWindow(hLblEncoding_, rx, y + 50, 76, 18, TRUE);
        ::MoveWindow(hCmbEncoding_, rx + 78, y + 46, 220, 200, TRUE);
        y += GH + 8;

        ::MoveWindow(hGrpWinMerge_, M, y, gw, GH, TRUE);
        ::MoveWindow(hLblWinMerge_, rx, y + 26, 130, 18, TRUE);
        int ww = rw - 130 - 80 - 12;
        if (ww < 100) ww = 100;
        ::MoveWindow(hEditWinMerge_, rx + 132, y + 22, ww, 22, TRUE);
        ::MoveWindow(hBtnWinMergeBrowse_, rx + 132 + ww + 6, y + 22, 74, 22, TRUE);
        y += GH + 8;

        ::MoveWindow(hGrpUndoOpt_, M, y, gw, 58, TRUE);
        ::MoveWindow(hLblUndoMax_, rx, y + 26, 150, 18, TRUE);
        ::MoveWindow(hEditUndoMax_, rx + 152, y + 22, 60, 22, TRUE);
        y += 58 + 8;

        int ih = H - y - M;
        if (ih < 18) ih = 18;
        ::MoveWindow(hLblSearchInfo_, M, y, gw, ih, TRUE);
        break;
    }
    case PG_RESULT: {
        int top = M;
        ::MoveWindow(hBtnSelectAll_, M, top, 100, 26, TRUE);
        ::MoveWindow(hBtnClearSel_,  M + 106, top, 100, 26, TRUE);
        ::MoveWindow(hBtnExportCsv_, M + 212, top, 92, 26, TRUE);
        ::MoveWindow(hBtnExportMd_,  M + 308, top, 92, 26, TRUE);
        ::MoveWindow(hBtnExportJson_, M + 404, top, 92, 26, TRUE);
        int iw = W - M * 2 - 506;
        if (iw < 60) iw = 60;
        ::MoveWindow(hLblResultInfo_, M + 506, top + 5, iw, 18, TRUE);
        ::MoveWindow(hListView_, M, top + 32, W - M * 2, H - top - 32 - M, TRUE);
        break;
    }
    case PG_PREVIEW: {
        int top = M;
        int gw = W - M * 2;
        // 1 行目: 表示モードと出力ボタン
        ::MoveWindow(hCmbPreviewMode_, M, top, 200, 200, TRUE);
        ::MoveWindow(hBtnWinMerge_, M + gw - 180, top, 180, 24, TRUE);
        ::MoveWindow(hBtnOpenHtml_, M + gw - 372, top, 184, 24, TRUE);
        // 2 行目: 比較する範囲（何回目の変更前 ～ 何回目の変更後）
        int row2 = top + 32;
        ::MoveWindow(hLblPairFrom_, M, row2 + 4, 96, 18, TRUE);
        int combW = (gw - 96 - 26 - 20) / 2;
        if (combW < 120) combW = 120;
        ::MoveWindow(hCmbPairFrom_, M + 98, row2, combW, 200, TRUE);
        ::MoveWindow(hLblPairTo_, M + 98 + combW + 6, row2 + 4, 20, 18, TRUE);
        ::MoveWindow(hCmbPairTo_, M + 98 + combW + 28, row2, combW, 200, TRUE);
        ::MoveWindow(hPreviewEdit_, M, row2 + 32, gw, H - row2 - 32 - M, TRUE);
        break;
    }
    case PG_ADD: {
        int gw = W - M * 2;
        int y = M;
        ::MoveWindow(hChkAddEnable_, M, y + 4, gw, 20, TRUE);
        y += 28;
        int lw = (gw - 20) / 2;
        int lh = (H - y - M - 44) / 2;
        if (lh < 60) lh = 60;
        ::MoveWindow(hLblAddBefore_, M, y, lw, 18, TRUE);
        ::MoveWindow(hLblAddAfter_, M + lw + 20, y, lw, 18, TRUE);
        y += 20;
        ::MoveWindow(hEditAddBefore_, M, y, lw, lh, TRUE);
        ::MoveWindow(hEditAddAfter_, M + lw + 20, y, lw, lh, TRUE);
        y += lh + 8;
        ::MoveWindow(hLblAddCode_, M, y, gw, 18, TRUE);
        y += 20;
        int ch = H - y - M - 34;
        if (ch < 50) ch = 50;
        ::MoveWindow(hEditAddCode_, M, y, gw, ch, TRUE);
        y += ch + 6;
        ::MoveWindow(hBtnAddSearch_, M, y, 190, 26, TRUE);
        ::MoveWindow(hLblAddInfo_, M + 200, y + 5, gw - 200, 18, TRUE);
        break;
    }
    case PG_HISTORY: {
        int gw = W - M * 2;
        int y = M;
        ::MoveWindow(hLblHistRun_, M, y + 4, 96, 18, TRUE);
        ::MoveWindow(hCmbHistRun_, M + 98, y, gw - 98 - 96 - 10, 200, TRUE);
        ::MoveWindow(hBtnHistRefresh_, M + gw - 90, y, 90, 24, TRUE);
        y += 30;
        int lh = (H - y - M - 40) / 3;
        if (lh < 80) lh = 80;
        ::MoveWindow(hListHistFiles_, M, y, gw, lh, TRUE);
        y += lh + 6;
        ::MoveWindow(hBtnHistRestore_, M, y, 210, 26, TRUE);
        ::MoveWindow(hBtnHistCompare_, M + 218, y, 150, 26, TRUE);
        ::MoveWindow(hBtnHistReport_, M + 376, y, 150, 26, TRUE);
        ::MoveWindow(hLblHistInfo_, M + 536, y + 5, gw - 536, 18, TRUE);
        y += 30;
        int eh = H - y - M;
        if (eh < 60) eh = 60;
        ::MoveWindow(hEditHistPreview_, M, y, gw, eh, TRUE);
        break;
    }
    case PG_SWITCH: {
        int gw = W - M * 2;
        int y = M;

        ::MoveWindow(hLblSwHeader_, M, y + 4, 120, 18, TRUE);
        int ew = gw - 120 - 74 - 74 - 18;
        if (ew < 120) ew = 120;
        ::MoveWindow(hSwHeaderEdit_, M + 120, y, ew, 24, TRUE);
        ::MoveWindow(hBtnSwHeaderBrowse_, M + 120 + ew + 6, y, 74, 24, TRUE);
        ::MoveWindow(hBtnSwHeaderLoad_, M + 120 + ew + 6 + 74 + 6, y, 74, 24, TRUE);
        y += 24 + 10;

        int gh = 178;
        ::MoveWindow(hGrpSwAuto_, M, y, gw, gh, TRUE);
        int rx = M + 14;
        int rw = gw - 28;
        ::MoveWindow(hChkManageSwHeader_, rx, y + 22, rw, 18, TRUE);
        ::MoveWindow(hChkAddInclude_,     rx, y + 44, rw, 18, TRUE);
        ::MoveWindow(hLblSwHeaderName_,   rx, y + 74, 110, 18, TRUE);
        ::MoveWindow(hEditSwHeaderName_,  rx + 110, y + 70, 220, 22, TRUE);
        ::MoveWindow(hLblSwHeaderDir_,    rx, y + 102, 110, 18, TRUE);
        int dw = rw - 110 - 80;
        if (dw < 80) dw = 80;
        ::MoveWindow(hEditSwHeaderDir_,   rx + 110, y + 98, dw, 22, TRUE);
        ::MoveWindow(hBtnSwHeaderDir_,    rx + rw - 74, y + 98, 74, 22, TRUE);
        ::MoveWindow(hLblSwMemo_,         rx, y + 130, 110, 18, TRUE);
        ::MoveWindow(hEditSwMemo_,        rx + 110, y + 126, rw - 110, 22, TRUE);
        y += gh + 8;

        ::MoveWindow(hBtnSwCreate_,  M,      y, 170, 26, TRUE);
        ::MoveWindow(hBtnSwEnable_,  M + 176, y,  84, 26, TRUE);
        ::MoveWindow(hBtnSwDisable_, M + 266, y,  84, 26, TRUE);
        ::MoveWindow(hBtnSwRemove_,  M + 356, y, 110, 26, TRUE);
        ::MoveWindow(hBtnSwOpen_,    M + 472, y, 120, 26, TRUE);
        int ix = M + 600;
        int iw = gw - 600;
        if (iw < 100) { ix = M + 472 + 120 + 10; iw = gw - (472 + 130); }
        if (iw < 60) { ix = M; iw = gw; }
        ::MoveWindow(hLblSwInfo_, ix, y + 5, iw, 18, TRUE);
        y += 26 + 6;

        int lh = H - y - M;
        if (lh < 60) lh = 60;
        ::MoveWindow(hListSwitches_, M, y, gw, lh, TRUE);
        break;
    }
    case PG_SYSTEM: {
        int gw = W - M * 2;
        int combW2 = (gw - 210) / 2;
        ::MoveWindow(hLblSysPairFrom_, M, M + 4, 104, 18, TRUE);
        ::MoveWindow(hCmbSysPairFrom_, M + 106, M, combW2, 200, TRUE);
        ::MoveWindow(hLblSysPairTo_, M + 106 + combW2 + 4, M + 4, 20, 18, TRUE);
        ::MoveWindow(hCmbSysPairTo_, M + 106 + combW2 + 26, M, combW2 - 26, 200, TRUE);
        int y = M + 30;

        ::MoveWindow(hLblSysName_, M, y + 4, 130, 18, TRUE);
        ::MoveWindow(hEditSysName_, M + 130, y, gw - 130, 24, TRUE);
        y += 24 + 6;

        int oh = 46;
        ::MoveWindow(hLblSysOverview_, M, y, 240, 18, TRUE);
        ::MoveWindow(hEditSysOverview_, M, y + 18, gw, oh, TRUE);
        y += oh + 18 + 6;

        int ph = 38;
        ::MoveWindow(hLblSysPurpose_, M, y, 240, 18, TRUE);
        ::MoveWindow(hEditSysPurpose_, M, y + 18, gw, ph, TRUE);
        y += ph + 18 + 6;

        int mh = 38;
        ::MoveWindow(hLblSysMod_, M, y, 240, 18, TRUE);
        ::MoveWindow(hEditSysMod_, M, y + 18, gw, mh, TRUE);
        y += mh + 18 + 6;

        int xh = 30;
        ::MoveWindow(hLblSysExtra_, M, y, 300, 18, TRUE);
        ::MoveWindow(hEditSysExtra_, M, y + 18, gw, xh, TRUE);
        y += xh + 18 + 6;

        // 行1: API で生成
        ::MoveWindow(hBtnSpecChange_, M, y, 150, 26, TRUE);
        ::MoveWindow(hBtnSpecInspect_, M + 156, y, 150, 26, TRUE);
        ::MoveWindow(hBtnSpecBoth_, M + 312, y, 120, 26, TRUE);
        ::MoveWindow(hChkAiOpenReport_, M + 440, y + 4, 190, 18, TRUE);
        ::MoveWindow(hBtnSpecOpen_, M + 636, y, 130, 26, TRUE);
        int lx = M + 774;
        int lw = gw - 774;
        if (lw < 80) { lx = M; lw = gw; }
        ::MoveWindow(hLblSpecInfo_, lx, y + 5, lw, 18, TRUE);
        y += 26 + 6;

        // 行2: プロンプト txt 出力 / 貼り付けた回答のレポート化
        ::MoveWindow(hBtnPromptChange_, M, y, 180, 26, TRUE);
        ::MoveWindow(hBtnPromptInspect_, M + 186, y, 180, 26, TRUE);
        ::MoveWindow(hBtnPromptBoth_, M + 372, y, 150, 26, TRUE);
        ::MoveWindow(hBtnMakeChange_, M + 528, y, 190, 26, TRUE);
        ::MoveWindow(hBtnMakeInspect_, M + 724, y, 190, 26, TRUE);
        y += 26 + 4;

        ::MoveWindow(hLblPasteNote_, M, y, gw, 18, TRUE);
        y += 20 + 2;

        ::MoveWindow(hLblSpecOutNote_, M, y, gw, 18, TRUE);
        y += 20;

        int lh = H - y - M;
        if (lh < 50) lh = 50;
        ::MoveWindow(hEditSpecOut_, M, y, gw, lh, TRUE);
        break;
    }
    case PG_LOG: {
        int top = M;
        ::MoveWindow(hBtnClearLog_, W - M - 110, top, 110, 26, TRUE);
        ::MoveWindow(hLogEdit_, M, top + 32, W - M * 2, H - top - 32 - M, TRUE);
        break;
    }
    default: break;
    }
}

void App::LayoutControls()
{
    RECT rc;
    ::GetClientRect(hWnd_, &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    if (hStatus_) ::SendMessageW(hStatus_, WM_SIZE, 0, 0);
    int statusH = 0;
    if (hStatus_) {
        RECT rs;
        ::GetWindowRect(hStatus_, &rs);
        statusH = rs.bottom - rs.top;
    }
    int bodyH = clientH - statusH;
    if (bodyH < 240) bodyH = 240;

    const int M = 10;
    const int LABEL_W = 92;
    const int ROW_H = 24;
    const int BTN_W = 74;
    const int GAP = 6;

    int y = M;
    ::MoveWindow(hLblFile_, M, y + 4, LABEL_W, 18, TRUE);
    int editW = clientW - M * 2 - LABEL_W - BTN_W - GAP * 2;
    if (editW < 140) editW = 140;
    ::MoveWindow(hTargetEdit_, M + LABEL_W, y, editW, ROW_H, TRUE);
    ::MoveWindow(hBrowseBtn_, M + LABEL_W + editW + GAP, y, BTN_W, ROW_H, TRUE);
    y += ROW_H + GAP;

    ::MoveWindow(hLblSwitch_, M, y + 4, LABEL_W, 18, TRUE);
    ::MoveWindow(hSwitchEdit_, M + LABEL_W, y, 260, ROW_H, TRUE);
    int hintX = M + LABEL_W + 270;
    int hintW = clientW - hintX - M;
    if (hintW < 100) hintW = 100;
    ::MoveWindow(hLblHintSwitch_, hintX, y + 4, hintW, 18, TRUE);
    y += ROW_H + GAP + 2;

    const int BTN_ROW_H = 30;
    int btnY = bodyH - M - BTN_ROW_H;
    int tabH = btnY - GAP - y;
    if (tabH < 200) tabH = 200;
    ::MoveWindow(hTab_, M, y, clientW - M * 2, tabH, TRUE);

    RECT disp;
    disp.left = M + 4; disp.top = y + 30; disp.right = clientW - M - 4;
    disp.bottom = y + tabH - 4;
    if (hTab_) {
        RECT tr;
        ::GetWindowRect(hTab_, &tr);
        ::MapWindowPoints(nullptr, hWnd_, (LPPOINT)&tr, 2);
        RECT item;
        item.left = item.top = item.right = item.bottom = 0;
        ::SendMessageW(hTab_, TCM_GETITEMRECT, 0, (LPARAM)&item);
        disp.left   = tr.left + 4;
        disp.top    = tr.top + item.bottom + 4;
        disp.right  = tr.right - 4;
        disp.bottom = tr.bottom - 4;
    }
    int pw = disp.right - disp.left;
    int ph = disp.bottom - disp.top;
    if (pw < 60) pw = 60;
    if (ph < 60) ph = 60;

    for (int i = 0; i < PG_COUNT; ++i) {
        ::MoveWindow(hPage_[i], disp.left, disp.top, pw, ph, TRUE);
        RECT pr;
        pr.left = 0; pr.top = 0; pr.right = pw; pr.bottom = ph;
        LayoutPage(i, pr);
    }

    int bx = M;
    ::MoveWindow(hBtnSearch_,  bx, btnY, BTN_W, BTN_ROW_H, TRUE); bx += BTN_W + GAP;
    ::MoveWindow(hBtnPreview_, bx, btnY, BTN_W, BTN_ROW_H, TRUE); bx += BTN_W + GAP;
    ::MoveWindow(hBtnDryRun_,  bx, btnY, BTN_W, BTN_ROW_H, TRUE); bx += BTN_W + GAP;
    ::MoveWindow(hBtnExecute_, bx, btnY, BTN_W, BTN_ROW_H, TRUE); bx += BTN_W + GAP;
    ::MoveWindow(hBtnUndo_,    bx, btnY, BTN_W, BTN_ROW_H, TRUE); bx += BTN_W + GAP;

    ::MoveWindow(hBtnExit_, clientW - M - BTN_W, btnY, BTN_W, BTN_ROW_H, TRUE);
    int pbW = 160;
    int pbX = clientW - M - BTN_W - GAP * 2 - pbW;
    if (pbX < bx) pbX = bx;
    ::MoveWindow(hProgress_, pbX, btnY + 7, pbW, 16, TRUE);

    if (hStatus_) {
        RECT rs;
        ::GetWindowRect(hStatus_, &rs);
        ::MoveWindow(hStatus_, 0, clientH - (rs.bottom - rs.top),
                     clientW, rs.bottom - rs.top, TRUE);
    }
}

// -----------------------------------------------------------------------------
// 設定ファイル ⇔ 画面
// -----------------------------------------------------------------------------
void App::LoadConfigToUi()
{
    LoadExtraUi();
    suppress_events_ = true;

    SetChecked(hChkBackup_,      cfg_.create_backup);
    SetChecked(hChkReport_,      cfg_.create_html_report);
    SetChecked(hChkLog_,         cfg_.create_log);
    SetChecked(hChkOpenReport_,  cfg_.open_report_after_execution);
    SetChecked(hChkDryRunOnly_,  cfg_.dry_run_only);
    SetChecked(hChkAutoIndent_,  cfg_.auto_indent);
    SetChecked(hChkWarnComment_, cfg_.warn_comment_match);
    SetChecked(hChkWarnPreproc_, cfg_.warn_existing_preprocessor);

    SetRadioGroup(hRadStyle_, 4, (int)cfg_.default_switch_style);
    SetRadioGroup(hRadHit_,   4, (int)cfg_.hit_behavior);
    SetRadioGroup(hRadTargetKind_, 2,
                  cfg_.target_kind == TargetKind::Folder ? 1 : 0);

    SetEditText(hEditTemplate_, cfg_.custom_template);
    SetEditText(hFolderEdit_, cfg_.last_target_folder);
    SetChecked(hChkSubdirs_, cfg_.include_subdirs);

    // 拡張子フィルタ
    std::vector<std::wstring> cand = FolderScanner::CandidateExtensions();
    for (size_t i = 0; i < cand.size() && i < 6; ++i) {
        bool on = false;
        for (size_t k = 0; k < cfg_.extension_filter.size(); ++k)
            if (util::ToUpper(cfg_.extension_filter[k]) == util::ToUpper(cand[i])) on = true;
        SetChecked(hChkExt_[i], on);
    }

    int pm = 0;
    switch (cfg_.preview_mode) {
    case PreviewMode::TopBottom:  pm = 0; break;
    case PreviewMode::Simple:     pm = 1; break;
    case PreviewMode::SideBySide: pm = 2; break;
    case PreviewMode::Html:       pm = 3; break;
    }
    ::SendMessageW(hCmbPreviewMode_, CB_SETCURSEL, pm, 0);

    SetChecked(hChkManageSwHeader_, cfg_.manage_switch_header);
    SetChecked(hChkAddInclude_,     cfg_.add_switch_header_include);
    SetEditText(hEditSwHeaderName_,
                cfg_.switch_header_name.empty() ? SwitchHeaderManager::DefaultHeaderName()
                                                : cfg_.switch_header_name);
    SetEditText(hEditSwHeaderDir_,  cfg_.switch_header_dir);
    SetEditText(hEditSwMemo_,       cfg_.switch_memo);

    // ---- AI 連携 ----
    ::SendMessageW(hCmbAiProvider_, CB_SETCURSEL,
                   (cfg_.ai_provider == 1) ? 1 : 0, 0);
    LoadAiFieldsFromConfig(cfg_.ai_provider);
    SetEditText(hEditAiTimeout_,  std::to_wstring(cfg_.ai_timeout_sec > 0 ? cfg_.ai_timeout_sec : 600));
    SetChecked(hChkAiOpenReport_, cfg_.ai_open_report);

    // ---- システム情報 ----
    SetEditText(hEditSysName_,      cfg_.sys_name);
    SetEditText(hEditSysOverview_,  cfg_.sys_overview);
    SetEditText(hEditSysPurpose_,   cfg_.sys_purpose);
    SetEditText(hEditSysMod_,       cfg_.sys_modification);
    SetEditText(hEditSysExtra_,     cfg_.sys_extra);
    last_spec_report_path_ = cfg_.last_spec_report_path;
    if (!last_spec_report_path_.empty())
        SetEditText(hLblSpecInfo_, L"前回の出力: " + last_spec_report_path_);

    suppress_events_ = false;
    OnTargetKindChanged();
    UpdateInputInfo();
    RefreshScanInfo();
    RefreshResolvedHeaderPath();
    if (util::PathExists(CurrentSwitchHeaderPath())) DoSwitchHeaderLoad();

    LoadSearchOptionsFromConfig();
}

void App::SaveUiToConfig()
{
    SaveExtraUi();
    cfg_.create_backup = IsChecked(hChkBackup_);
    cfg_.create_html_report = IsChecked(hChkReport_);
    cfg_.create_log = IsChecked(hChkLog_);
    cfg_.open_report_after_execution = IsChecked(hChkOpenReport_);
    cfg_.dry_run_only = IsChecked(hChkDryRunOnly_);
    cfg_.auto_indent = IsChecked(hChkAutoIndent_);
    cfg_.warn_comment_match = IsChecked(hChkWarnComment_);
    cfg_.warn_existing_preprocessor = IsChecked(hChkWarnPreproc_);
    cfg_.default_switch_style = ReadStyleFromUi();
    cfg_.hit_behavior = ReadHitBehaviorFromUi();
    cfg_.preview_mode = ReadPreviewModeFromUi();
    cfg_.custom_template = GetEditText(hEditTemplate_);
    if (!target_path_.empty())
        cfg_.last_target_directory = util::GetDirectoryOf(target_path_);

    cfg_.target_kind = ReadTargetKindFromUi();
    std::wstring folder = util::Trim(GetEditText(hFolderEdit_));
    if (!folder.empty()) cfg_.last_target_folder = folder;
    cfg_.extension_filter = ReadExtensionsFromUi();
    cfg_.include_subdirs = IsChecked(hChkSubdirs_);

    cfg_.manage_switch_header = IsChecked(hChkManageSwHeader_);
    cfg_.add_switch_header_include = IsChecked(hChkAddInclude_);
    std::wstring hn = util::Trim(GetEditText(hEditSwHeaderName_));
    cfg_.switch_header_name = hn.empty() ? SwitchHeaderManager::DefaultHeaderName() : hn;
    cfg_.switch_header_dir = util::Trim(GetEditText(hEditSwHeaderDir_));
    cfg_.switch_memo = util::Trim(GetEditText(hEditSwMemo_));
    cfg_.last_switch_header_path = CurrentSwitchHeaderPath();

    // ---- AI 連携 ----
    cfg_.ai_provider = (int)::SendMessageW(hCmbAiProvider_, CB_GETCURSEL, 0, 0);
    if (cfg_.ai_provider < 0 || cfg_.ai_provider > 1) cfg_.ai_provider = 0;
    SaveAiFieldsToConfig(cfg_.ai_provider);
    if (cfg_.ai_task_type.empty()) cfg_.ai_task_type = AiClient::DefaultTaskType();
    int tmo = ::_wtoi(util::Trim(GetEditText(hEditAiTimeout_)).c_str());
    if (tmo < 30) tmo = 30;
    if (tmo > 3600) tmo = 3600;
    cfg_.ai_timeout_sec = tmo;
    cfg_.ai_open_report = IsChecked(hChkAiOpenReport_);

    // ---- システム情報 ----
    cfg_.sys_name         = util::Trim(GetEditText(hEditSysName_));
    cfg_.sys_overview     = util::Trim(GetEditText(hEditSysOverview_));
    cfg_.sys_purpose      = util::Trim(GetEditText(hEditSysPurpose_));
    cfg_.sys_modification = util::Trim(GetEditText(hEditSysMod_));
    cfg_.sys_extra        = util::Trim(GetEditText(hEditSysExtra_));
    cfg_.last_spec_report_path = last_spec_report_path_;

    cfgmgr_.Save(cfg_);

    SaveSearchOptionsToConfig();
}

// -----------------------------------------------------------------------------
// メッセージ処理
// -----------------------------------------------------------------------------
LRESULT App::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        OnCreate();
        LayoutControls();
        return 0;

    case WM_SIZE:
        OnSize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 980;
        mmi->ptMinTrackSize.y = 740;
        return 0;
    }

    case WM_COMMAND:
        OnCommand(LOWORD(wp), HIWORD(wp), (HWND)lp);
        return 0;

    case WM_NOTIFY:
        OnNotify((NMHDR*)lp);
        return 0;

    case WM_DROPFILES:
        OnDropFiles((HDROP)wp);
        return 0;

    case WM_CLOSE:
        if (state_ == AppState::EXECUTING) return 0;
        ::DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        OnDestroy();
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wp, lp);
}

void App::OnSize(int cx, int cy)
{
    (void)cx; (void)cy;
    LayoutControls();
}

void App::OnDestroy()
{
    SaveUiToConfig();
    log_.Info(L"Application terminated.");
    log_.Shutdown();
    ::PostQuitMessage(0);
}

void App::OnDropFiles(HDROP hDrop)
{
    wchar_t buf[MAX_PATH * 2];
    UINT n = ::DragQueryFileW(hDrop, 0, buf, MAX_PATH * 2);
    ::DragFinish(hDrop);
    if (n == 0) return;

    DWORD attr = ::GetFileAttributesW(buf);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        // フォルダがドロップされた場合は一括処理モードへ
        SetRadioGroup(hRadTargetKind_, 2, 1);
        SetEditText(hFolderEdit_, buf);
        OnTargetKindChanged();
        DoScan();
        return;
    }

    SetRadioGroup(hRadTargetKind_, 2, 0);
    OnTargetKindChanged();
    SetEditText(hTargetEdit_, buf);
    target_path_ = util::MakeAbsolute(buf);
    file_text_.clear();
    matches_.clear();
    RefreshListView();
    DoBrowse();
}

void App::OnTabChange()
{
    int sel = (int)::SendMessageW(hTab_, TCM_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= PG_COUNT) return;
    for (int i = 0; i < PG_COUNT; ++i)
        ::ShowWindow(hPage_[i], (i == sel) ? SW_SHOW : SW_HIDE);
    // 起動直後やタブ切替時に画面が描画されない問題への対策。
    // ShowWindow の直後にページとすべての子ウィンドウへ WM_PAINT を発生させる。
    RECT pr;
    ::GetClientRect(hPage_[sel], &pr);
    ::InvalidateRect(hPage_[sel], &pr, TRUE);
    ::UpdateWindow(hPage_[sel]);
    ::RedrawWindow(hPage_[sel], nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    ::RedrawWindow(hWnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (sel == PG_RESULT) UpdateSelectionColumn();
    if (sel == PG_SYSTEM && !last_spec_report_path_.empty())
        SetEditText(hLblSpecInfo_, L"出力: " + last_spec_report_path_);
    if (sel == PG_SWITCH) {
        RefreshResolvedHeaderPath();
        if (util::PathExists(CurrentSwitchHeaderPath())) DoSwitchHeaderLoad();
    }
}

void App::OnNotify(NMHDR* nm)
{
    if (!nm) return;
    switch (nm->code) {
    case TCN_SELCHANGE:
        if (nm->idFrom == IDC_TAB_MAIN) OnTabChange();
        break;

    case LVN_ITEMCHANGED: {
        if (suppress_events_) break;
        NMLISTVIEW* nv = (NMLISTVIEW*)nm;
        if (!(nv->uChanged & LVIF_STATE)) break;
        UINT oldState = nv->uOldState & LVIS_STATEIMAGEMASK;
        UINT newState = nv->uNewState & LVIS_STATEIMAGEMASK;
        if (oldState == newState) break;
        if (nm->idFrom == IDC_LIST_MATCHES) SyncSelectionFromList();
        else if (nm->idFrom == IDC_LIST_FILES) SyncFileSelectionFromList();
        break;
    }

    case NM_DBLCLK:
        if (nm->idFrom == IDC_LIST_MATCHES && !matches_.empty()) DoPreview();
        break;

    default:
        break;
    }
}

void App::OnCommand(int id, int code, HWND ctrl)
{
    if (HandleExtraCommand(id, code, ctrl)) return;
    // ---- 追加機能: ログ / レポート作成単位 / GoogleTest ----
    switch (id) {
    case IDC_CHK_LOGENABLED:
    case IDC_CHK_LOGTEMP:
    case IDC_CHK_LOGRET:
    case IDC_CHK_GTEST:
        if (!suppress_events_) SaveBasicExtrasToConfig();
        break;
    case IDC_CMB_REPORTSCOPE:
        if (code == CBN_SELCHANGE && !suppress_events_) SaveBasicExtrasToConfig();
        break;
    case IDC_EDIT_LOADITER:
        if (code == EN_KILLFOCUS && !suppress_events_) SaveBasicExtrasToConfig();
        break;
    case IDC_BTN_LOGHEADER:
        DoWriteLogHeader();
        return;
    case IDC_BTN_GTESTGEN:
        DoGenerateGTest();
        return;
    case IDC_BTN_GTESTRUN:
        DoRunGTest();
        return;
    default:
        break;
    }

    // ---- オプションのチェック状態を即座に保存する ----
    // （保存は終了時のみだったため、チェックを付けても次回起動時に戻っていた）
    if (!suppress_events_ &&
        (id == IDC_CHK_WARNCOMMENT || id == IDC_CHK_WARNPREPROC ||
         id == IDC_CHK_BACKUP || id == IDC_CHK_REPORT || id == IDC_CHK_LOG ||
         id == IDC_CHK_OPENREPORT || id == IDC_CHK_DRYRUNONLY || id == IDC_CHK_AUTOINDENT)) {
        SaveUiToConfig();
    }

    // ---- 後続版: 検索条件 / エクスポート / WinMerge 連携 ----
    if (id == IDC_CHK_SIMILAR || id == IDC_CHK_LINERANGE || id == IDC_CHK_FUNCSCOPE ||
        id == IDC_CHK_REPORTMD || id == IDC_CHK_REPORTJSON) {
        SaveSearchOptionsToConfig();
        return;
    }
    if (id == IDC_CMB_ENCODING) {
        if (code == CBN_SELCHANGE) SaveSearchOptionsToConfig();
        return;
    }
    if (id == IDC_BTN_EXPORTCSV)      { DoExportResults(0); return; }
    if (id == IDC_BTN_EXPORTMD)       { DoExportResults(1); return; }
    if (id == IDC_BTN_EXPORTJSON)     { DoExportResults(2); return; }
    if (id == IDC_BTN_WINMERGE)       { DoWinMerge(); return; }
    if (id == IDC_BTN_WINMERGEBROWSE) { DoWinMergeBrowse(); return; }

    (void)ctrl;

    switch (id) {
    case IDC_BTN_BROWSE:        if (code == BN_CLICKED) DoBrowse();          return;
    case IDC_BTN_FOLDERBROWSE:  if (code == BN_CLICKED) DoFolderBrowse();    return;
    case IDC_BTN_SCAN:          if (code == BN_CLICKED) DoScan();            return;
    case IDC_BTN_SEARCH:        if (code == BN_CLICKED) DoSearch();          return;
    case IDC_BTN_PREVIEW:       if (code == BN_CLICKED) DoPreview();         return;
    case IDC_BTN_DRYRUN:        if (code == BN_CLICKED) DoDryRun();          return;
    case IDC_BTN_EXECUTE:       if (code == BN_CLICKED) DoExecute();         return;
    case IDC_BTN_UNDO:          if (code == BN_CLICKED) DoUndo();            return;
    case IDC_BTN_EXIT:          if (code == BN_CLICKED) ::PostMessageW(hWnd_, WM_CLOSE, 0, 0); return;
    case IDC_BTN_SELECTALL:     if (code == BN_CLICKED) DoSelectAll();       return;
    case IDC_BTN_CLEARSEL:      if (code == BN_CLICKED) DoClearSelection();  return;
    case IDC_BTN_FILESELALL:    if (code == BN_CLICKED) DoFileSelectAll();   return;
    case IDC_BTN_FILESELNONE:   if (code == BN_CLICKED) DoFileSelectNone();  return;
    case IDC_BTN_CLEARLOG:      if (code == BN_CLICKED) DoClearLog();        return;
    case IDC_BTN_OPENHTML:      if (code == BN_CLICKED) DoOpenHtml();        return;
    case IDC_BTN_SWHEADERBROWSE: if (code == BN_CLICKED) DoSwitchHeaderBrowse(); return;
    case IDC_BTN_SWHEADERDIR:    if (code == BN_CLICKED) DoSwitchHeaderDirBrowse(); return;
    case IDC_BTN_SWHEADERLOAD:   if (code == BN_CLICKED) DoSwitchHeaderLoad();   return;
    case IDC_BTN_SWENABLE:       if (code == BN_CLICKED) DoSwitchEnable();       return;
    case IDC_BTN_SWDISABLE:      if (code == BN_CLICKED) DoSwitchDisable();      return;
    case IDC_BTN_SWREMOVE:       if (code == BN_CLICKED) DoSwitchRemove();       return;
    case IDC_BTN_SWOPEN:         if (code == BN_CLICKED) DoSwitchOpen();         return;
    case IDC_BTN_SWCREATE:       if (code == BN_CLICKED) DoSwitchCreateNow();    return;
    case IDC_BTN_SPECCHANGE:     if (code == BN_CLICKED) DoGenerateSpec(SPEC_CHANGE);     return;
    case IDC_BTN_SPECINSPECT:    if (code == BN_CLICKED) DoGenerateSpec(SPEC_INSPECTION); return;
    case IDC_BTN_SPECBOTH:       if (code == BN_CLICKED) DoGenerateSpecBoth();            return;
    case IDC_BTN_SPECOPEN:       if (code == BN_CLICKED) DoOpenSpecReport();              return;
    case IDC_BTN_AITEST:         if (code == BN_CLICKED) DoAiTest();                      return;
    case IDC_BTN_PROMPTCHANGE:   if (code == BN_CLICKED) DoExportPrompt(SPEC_CHANGE);      return;
    case IDC_BTN_PROMPTINSPECT:  if (code == BN_CLICKED) DoExportPrompt(SPEC_INSPECTION);  return;
    case IDC_BTN_PROMPTBOTH:     if (code == BN_CLICKED) DoExportPromptBoth();             return;
    case IDC_BTN_MAKECHANGE:     if (code == BN_CLICKED) DoMakeReportFromText(SPEC_CHANGE);     return;
    case IDC_BTN_MAKEINSPECT:    if (code == BN_CLICKED) DoMakeReportFromText(SPEC_INSPECTION); return;
    default: break;
    }

    // ラジオボタン: ダイアログ以外のウィンドウでは自動的に他が外れないため、
    // ここで明示的にグループのチェックを付け替える。
    if (id >= IDC_RAD_STYLE_A && id <= IDC_RAD_STYLE_D) {
        if (code == BN_CLICKED) SetRadioGroup(hRadStyle_, 4, id - IDC_RAD_STYLE_A);
        return;
    }
    if (id >= IDC_RAD_HIT_AUTO && id <= IDC_RAD_HIT_MANUAL) {
        if (code == BN_CLICKED) SetRadioGroup(hRadHit_, 4, id - IDC_RAD_HIT_AUTO);
        return;
    }
    if (id == IDC_CMB_AIPROVIDER) {
        if (code == CBN_SELCHANGE) DoAiProviderChanged();
        return;
    }

    if (id == IDC_RAD_TARGET_FILE || id == IDC_RAD_TARGET_FOLDER) {
        if (code == BN_CLICKED) {
            SetRadioGroup(hRadTargetKind_, 2, (id == IDC_RAD_TARGET_FOLDER) ? 1 : 0);
            OnTargetKindChanged();
        }
        return;
    }

    if (id == IDC_EDIT_OLDCODE || id == IDC_EDIT_NEWCODE) {
        if (code == EN_CHANGE) { UpdateInputInfo(); return; }
    }
    if (id == IDC_EDIT_SWITCHNAME && code == EN_CHANGE) {
        if (suppress_events_) return;
        std::wstring name = util::Trim(GetEditText(hSwitchEdit_));
        if (name.empty()) {
            SetEditText(hLblHintSwitch_, L"");
        } else {
            ValidationResult vr = ValidationManager::ValidateSwitchName(name);
            if (!vr.ok) SetEditText(hLblHintSwitch_, L"× " + vr.error);
            else if (!vr.warnings.empty()) SetEditText(hLblHintSwitch_, L"△ " + vr.warnings[0]);
            else SetEditText(hLblHintSwitch_, L"○ 使用可能なスイッチ名です");
        }
        return;
    }
    if (id == IDC_CMB_PREVIEWMODE && code == CBN_SELCHANGE) {
        if (suppress_events_) return;
        DoPreviewModeChanged();
        return;
    }
    if (id == IDC_CHK_DRYRUNONLY && code == BN_CLICKED) {
        UpdateButtons();
        return;
    }
    if (id == IDC_CHK_LOG && code == BN_CLICKED) {
        log_.SetEnabled(IsChecked(hChkLog_));
        return;
    }
    if ((id == IDC_EDIT_SWHEADERNAME || id == IDC_EDIT_SWHEADERDIR) && code == EN_CHANGE) {
        if (suppress_events_) return;
        RefreshResolvedHeaderPath();
        return;
    }
    if (id == IDC_CHK_MANAGESWHEADER || id == IDC_CHK_ADDINCLUDE) {
        if (code == BN_CLICKED) {
            bool on = IsChecked(hChkManageSwHeader_);
            ::EnableWindow(hChkAddInclude_, on);
            ::EnableWindow(hEditSwHeaderName_, on);
            ::EnableWindow(hEditSwHeaderDir_, on);
            ::EnableWindow(hBtnSwHeaderDir_, on);
            ::EnableWindow(hEditSwMemo_, on);
            ::EnableWindow(hBtnSwCreate_, on);
        }
        return;
    }
}

// -----------------------------------------------------------------------------
// 小物ヘルパ
// -----------------------------------------------------------------------------
std::wstring App::GetEditText(HWND h) const
{
    if (!h) return std::wstring();
    int len = ::GetWindowTextLengthW(h);
    if (len <= 0) return std::wstring();
    std::wstring s((size_t)len + 1, L'\0');
    ::GetWindowTextW(h, &s[0], len + 1);
    s.resize((size_t)len);
    return s;
}

void App::SetEditText(HWND h, const std::wstring& s)
{
    if (h) ::SetWindowTextW(h, s.c_str());
}

bool App::IsChecked(HWND h) const
{
    if (!h) return false;
    return ::SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void App::SetChecked(HWND h, bool v)
{
    if (h) ::SendMessageW(h, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
}

void App::SetRadioGroup(HWND* group, int count, int index)
{
    if (index < 0 || index >= count) index = 0;
    for (int i = 0; i < count; ++i)
        if (group[i]) ::SendMessageW(group[i], BM_SETCHECK,
                                     (i == index) ? BST_CHECKED : BST_UNCHECKED, 0);
}

void App::SetStatus(const std::wstring& s)
{
    if (hStatus_) ::SendMessageW(hStatus_, SB_SETTEXTW, 0, (LPARAM)s.c_str());
}

void App::SetProgress(int percent)
{
    if (!hProgress_) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    ::SendMessageW(hProgress_, PBM_SETPOS, (WPARAM)percent, 0);
}

void App::AppendLog(const std::wstring& line)
{
    if (!hLogEdit_) return;
    std::wstring text = GetEditText(hLogEdit_);
    text += line;
    text += L"\r\n";
    if (text.size() > 400000) text.erase(0, text.size() - 300000);
    ::SetWindowTextW(hLogEdit_, text.c_str());
    int len = ::GetWindowTextLengthW(hLogEdit_);
    ::SendMessageW(hLogEdit_, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    ::SendMessageW(hLogEdit_, EM_SCROLLCARET, 0, 0);
}

void App::ShowError(const std::wstring& m)
{
    ::MessageBoxW(hWnd_, m.c_str(), L"CopyMigration Switcher - エラー",
                  MB_ICONERROR | MB_OK);
}

void App::ShowWarn(const std::wstring& m)
{
    ::MessageBoxW(hWnd_, m.c_str(), L"CopyMigration Switcher - 警告",
                  MB_ICONWARNING | MB_OK);
}

void App::ShowInfo(const std::wstring& m)
{
    ::MessageBoxW(hWnd_, m.c_str(), L"CopyMigration Switcher",
                  MB_ICONINFORMATION | MB_OK);
}

bool App::Confirm(const std::wstring& m)
{
    return ::MessageBoxW(hWnd_, m.c_str(), L"CopyMigration Switcher - 確認",
                         MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES;
}

void App::OpenPath(const std::wstring& p)
{
    if (p.empty()) return;
    ::ShellExecuteW(hWnd_, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::UpdateInputInfo()
{
    std::wstring oldc = util::NormalizeNewlines(GetEditText(hOldEdit_));
    std::wstring newc = util::NormalizeNewlines(GetEditText(hNewEdit_));

    wchar_t buf[256];
    swprintf(buf, 256, L"行数: %d / 文字数: %d（タブ・半角スペースはそのまま保持されます）",
             util::CountLines(oldc), (int)oldc.size());
    SetEditText(hLblOldInfo_, buf);

    swprintf(buf, 256, L"行数: %d / 文字数: %d",
             util::CountLines(newc), (int)newc.size());
    SetEditText(hLblNewInfo_, buf);
}


// =============================================================================
// 追加コードタブ
// =============================================================================
void App::CreateAddCodePage()
{
    HWND p = hPage_[PG_ADD];

    hChkAddEnable_ = MakeCheck(p, L"追加コードモードを使う（「直前」と「直後」の間にコードを挿入する）",
                               IDC_CHK_ADDENABLE, hFontUi_);
    hLblAddBefore_ = MakeStatic(p, L"直前のコード（この直後に挿入されます）", IDC_LBL_ADDBEFORE, hFontUi_);
    hEditAddBefore_ = MakeEdit(p, IDC_EDIT_ADDBEFORE,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontMono_);
    hLblAddAfter_ = MakeStatic(p, L"直後のコード（一致の判定に使います / 空でも可）", IDC_LBL_ADDAFTER, hFontUi_);
    hEditAddAfter_ = MakeEdit(p, IDC_EDIT_ADDAFTER,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontMono_);
    hLblAddCode_ = MakeStatic(p, L"追加するコード（既存コードのインデントに合わせて整形されます）",
                              IDC_LBL_ADDCODE, hFontUi_);
    hEditAddCode_ = MakeEdit(p, IDC_EDIT_ADDCODE,
        ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, hFontMono_);
    hBtnAddSearch_ = MakeButton(p, L"追加位置を検索", IDC_BTN_ADDSEARCH, BS_PUSHBUTTON, hFontUi_);
    hLblAddInfo_ = MakeStatic(p, L"", IDC_LBL_ADDINFO, hFontUi_);
}

// =============================================================================
// 履歴タブ（undobak の参照 / 特定変更の復元）
// =============================================================================
void App::CreateHistoryPage()
{
    HWND p = hPage_[PG_HISTORY];

    hLblHistRun_ = MakeStatic(p, L"実行履歴", IDC_LBL_HISTRUN, hFontUi_);
    hCmbHistRun_ = ::CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 10, 200, p, (HMENU)IDC_CMB_HISTRUN, hInst_, nullptr);
    ApplyFont(hCmbHistRun_);
    hBtnHistRefresh_ = MakeButton(p, L"更新", IDC_BTN_HISTREFRESH, BS_PUSHBUTTON, hFontUi_);

    hListHistFiles_ = ::CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        0, 0, 10, 10, p, (HMENU)IDC_LIST_HISTFILES, hInst_, nullptr);
    ApplyFont(hListHistFiles_);
    ::SendMessageW(hListHistFiles_, LVM_SETEXTENDEDLISTVIEWSTYLE,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES,
                   LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    struct Col { const wchar_t* name; int width; };
    static const Col cols[] = {
        { L"No", 44 }, { L"ファイル", 420 }, { L"変更前", 80 }, { L"変更後", 80 }
    };
    for (int i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); ++i) {
        LVCOLUMNW c;
        ::ZeroMemory(&c, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = (LPWSTR)cols[i].name;
        c.cx = cols[i].width;
        c.iSubItem = i;
        ::SendMessageW(hListHistFiles_, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }

    hBtnHistRestore_ = MakeButton(p, L"この変更だけ元に戻す", IDC_BTN_HISTRESTORE, BS_PUSHBUTTON, hFontUi_);
    hBtnHistCompare_ = MakeButton(p, L"WinMerge で比較", IDC_BTN_HISTCOMPARE, BS_PUSHBUTTON, hFontUi_);
    hBtnHistReport_  = MakeButton(p, L"レポート作成", IDC_BTN_HISTREPORT, BS_PUSHBUTTON, hFontUi_);
    hLblHistInfo_    = MakeStatic(p, L"", IDC_LBL_HISTINFO, hFontUi_);

    hEditHistPreview_ = MakeEdit(p, IDC_EDIT_HISTPREVIEW,
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        WS_VSCROLL | WS_HSCROLL, hFontMono_);
}

} // namespace ms

// =============================================================================
// main.cpp
// エントリポイント（設計書 9.1 アプリ起動時フロー）
// =============================================================================
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "App.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // 9.1 手順2: Common Controls 初期化
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_BAR_CLASSES |
                 ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    ::InitCommonControlsEx(&icc);

    // 高 DPI 対応
    typedef BOOL (WINAPI *SetDpiFn)(void);
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetDpiFn fn = (SetDpiFn)(void*)::GetProcAddress(user32, "SetProcessDPIAware");
        if (fn) fn();
    }

    ms::App app;
    if (!app.Initialize(hInstance, nCmdShow)) {
        ::MessageBoxW(nullptr, L"アプリケーションの初期化に失敗しました。",
                      L"CopyMigration Switcher", MB_ICONERROR | MB_OK);
        return 1;
    }
    return app.Run();
}

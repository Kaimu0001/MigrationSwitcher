// =============================================================================
// LogManager.h / LogManager.cpp
// 設計書 6.14「実行ログ機能」/ 7.2.10 LogManager
// =============================================================================
#pragma once

#include <string>
#include <vector>
#include <functional>

#include "Models.h"

namespace ms {

// ログ 1 行を UI へも流すためのコールバック
typedef std::function<void(const std::wstring&)> LogSink;

class LogManager {
public:
    LogManager();
    ~LogManager();

    // 出力先を <appDir>\logs に初期化する
    void Initialize(const std::wstring& app_dir, bool enabled);
    void Shutdown();

    void SetSink(const LogSink& sink) { sink_ = sink; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    void Write(LogLevel level, const std::wstring& message);
    void Debug(const std::wstring& m) { Write(LogLevel::DEBUG, m); }
    void Info(const std::wstring& m)  { Write(LogLevel::INFO,  m); }
    void Warn(const std::wstring& m)  { Write(LogLevel::WARN,  m); }
    void Error(const std::wstring& m) { Write(LogLevel::Error, m); }
    void Fatal(const std::wstring& m) { Write(LogLevel::FATAL, m); }

    // UI へだけ流す（ファイル出力しない）
    void ToUi(const std::wstring& m);

    static const wchar_t* LevelName(LogLevel level);

private:
    void OpenTodayFile();

    std::wstring  log_dir_;
    std::wstring  current_date_;
    void*         file_ = nullptr;   // HANDLE
    bool          enabled_ = true;
    LogSink       sink_;
};

} // namespace ms

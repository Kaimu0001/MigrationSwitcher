// =============================================================================
// LogManager.cpp
// =============================================================================
#include "LogManager.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

LogManager::LogManager() {}
LogManager::~LogManager() { Shutdown(); }

const wchar_t* LogManager::LevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::DEBUG: return L"DEBUG";
    case LogLevel::INFO:  return L"INFO";
    case LogLevel::WARN:  return L"WARN";
    case LogLevel::Error: return L"ERROR";
    case LogLevel::FATAL: return L"FATAL";
    }
    return L"INFO";
}

void LogManager::Initialize(const std::wstring& app_dir, bool enabled)
{
    enabled_ = enabled;
    log_dir_ = util::JoinPath(app_dir, L"logs");
    util::EnsureDirectory(log_dir_);
    OpenTodayFile();
}

void LogManager::Shutdown()
{
    if (file_) {
        ::CloseHandle((HANDLE)file_);
        file_ = nullptr;
    }
}

void LogManager::OpenTodayFile()
{
    std::wstring today = util::TimestampDate();
    if (file_ && today == current_date_) return;
    if (file_) {
        ::CloseHandle((HANDLE)file_);
        file_ = nullptr;
    }
    util::EnsureDirectory(log_dir_);
    std::wstring path = util::JoinPath(log_dir_, today + L".log");
    HANDLE h = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    // UTF-8 BOM を先頭に付与（新規作成時のみ）
    DWORD size = ::GetFileSize(h, nullptr);
    if (size == 0) {
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        DWORD written = 0;
        ::WriteFile(h, bom, 3, &written, nullptr);
    }
    file_ = (void*)h;
    current_date_ = today;
}

void LogManager::Write(LogLevel level, const std::wstring& message)
{
    std::wstring line = util::NowDateTimeMillis() + L" [" +
                        LevelName(level) + L"] " + message;

    if (sink_) sink_(line);

    if (!enabled_) return;
    OpenTodayFile();
    if (!file_) return;

    std::string utf8 = util::WideToUtf8(line + L"\r\n");
    DWORD written = 0;
    ::WriteFile((HANDLE)file_, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
}

void LogManager::ToUi(const std::wstring& m)
{
    if (sink_) sink_(m);
}

} // namespace ms

// =============================================================================
// ConfigManager.h
// 設計書 8.1「設定情報」/ 11.2「設定ファイル例」/ 7.2.11 ConfigManager
// =============================================================================
#pragma once

#include <string>

#include "Models.h"

namespace ms {

class ConfigManager {
public:
    ConfigManager();

    // config.ini のフルパスを指定して読み書きする
    void SetPath(const std::wstring& ini_path);
    const std::wstring& Path() const { return ini_path_; }

    void Load(AppConfig& config);
    void Save(const AppConfig& config);

    static std::wstring DefaultIniPath();

private:
    std::wstring ini_path_;
};

} // namespace ms

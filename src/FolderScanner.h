// =============================================================================
// FolderScanner.h
// 設計書 17.2.2「処理仕様」手順1-2 フォルダ配下の走査と拡張子フィルタ
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

struct ScanResult {
    std::vector<std::wstring> files;                    // 抽出された対象ファイル
    std::vector<std::wstring> skipped;                  // サイズ過大などで除外したファイル
    std::vector<std::wstring> errors;                   // 走査中のエラー
    int  scanned_directories = 0;
    bool truncated = false;                             // 上限件数に達した
};

class FolderScanner {
public:
    // 上限（暴走防止）
    static const int          kMaxFiles;        // 20000
    static const unsigned long long kMaxFileSize; // 8MB

    // 手順1: フォルダ配下を走査 / 手順2: 拡張子フィルタで対象抽出
    // extensions は L".c" 形式。空の場合は DefaultExtensions() を使用する。
    static void Scan(const std::wstring& folder,
                     const std::vector<std::wstring>& extensions,
                     bool include_subdirs,
                     ScanResult& out);

    // 設計書 17.2.1 の拡張子フィルタ初期値
    static std::vector<std::wstring> DefaultExtensions();
    // チェックボックスに並べる候補
    static std::vector<std::wstring> CandidateExtensions();

    // 拡張子がフィルタに含まれるか（大文字小文字を区別しない）
    static bool MatchesExtension(const std::wstring& path,
                                 const std::vector<std::wstring>& extensions);

    // 拡張子リストを "c,cpp,h" 形式へ／から変換（config.ini 保存用）
    static std::wstring JoinExtensions(const std::vector<std::wstring>& extensions);
    static std::vector<std::wstring> SplitExtensions(const std::wstring& text);

private:
    static void ScanRecursive(const std::wstring& dir,
                              const std::vector<std::wstring>& extensions,
                              bool include_subdirs,
                              ScanResult& out);
};

} // namespace ms

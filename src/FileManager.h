// =============================================================================
// FileManager.h
// 設計書 6.1「対象ファイル選択機能」/ 7.2.2 FileManager / 19.1 ファイル破損防止
// =============================================================================
#pragma once

#include <string>

#include "Models.h"

namespace ms {

class FileManager {
public:
    // 6.1.3 ファイル存在確認・読み書き権限・サイズ・文字コード・改行コード判定
    static FileInfo Inspect(const std::wstring& path, const std::wstring& encoding_mode);

    // 生バイト列の読み込み
    static bool ReadAllBytes(const std::wstring& path, std::string& out_bytes);

    // 内部表現（改行 LF 正規化済み）で読み込む
    static bool ReadText(const std::wstring& path,
                         const std::wstring& encoding_mode,
                         std::wstring& out_text,
                         FileInfo& out_info);

    // 19.1 一時ファイルへ書き出してから置換する（原子的書き込み）
    static bool WriteTextAtomic(const std::wstring& path,
                                const std::wstring& text_lf_normalized,
                                TextEncoding encoding,
                                bool with_bom,
                                NewlineKind newline,
                                std::wstring& out_error);

    // 単純コピー（Undo 復元・バックアップ用）
    static bool CopyFileTo(const std::wstring& from, const std::wstring& to,
                         std::wstring& out_error);

    // 一時ファイルを作成してフルパスを返す
    static std::wstring MakeTempFilePath(const std::wstring& base_name,
                                         const std::wstring& ext);

    static bool RemoveFile(const std::wstring& path);
    static std::wstring LastErrorMessage(unsigned long err);
    static bool IsReadOnlyAttribute(const std::wstring& path);
};

} // namespace ms

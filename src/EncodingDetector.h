// =============================================================================
// EncodingDetector.h
// 設計書 3.4「文字コード対応」/ 7.2.3 EncodingDetector
// 対応: UTF-8 / UTF-8 BOM 付き / Shift_JIS （UTF-16LE は BOM 付きのみ読込対応）
// =============================================================================
#pragma once

#include <string>

#include "Models.h"

namespace ms {

class EncodingDetector {
public:
    // BOM 判定 + ヒューリスティック判定
    // raw_bytes を解析して encoding を返す（BOM 長は bom_length に格納）
    static TextEncoding Detect(const std::string& raw_bytes, size_t& bom_length);

    // 生バイト列を内部表現（UTF-16 / std::wstring）へ変換する
    // force_mode: L"auto" / L"utf8" / L"utf8bom" / L"sjis"
    static bool Decode(const std::string& raw_bytes,
                       const std::wstring& force_mode,
                       std::wstring& out_text,
                       TextEncoding& out_encoding);

    // 内部表現を元の文字コード・BOM 有無へ再変換する
    static bool Encode(const std::wstring& text,
                       TextEncoding encoding,
                       bool with_bom,
                       std::string& out_bytes);

    static const wchar_t* EncodingName(TextEncoding encoding);

    // UTF-8 として妥当か（MB_ERR_INVALID_CHARS による厳密判定）
    static bool IsValidUtf8(const std::string& bytes);
    // Shift_JIS(CP932) として妥当か
    static bool IsValidShiftJis(const std::string& bytes);
    // EUC-JP として妥当か（内蔵テーブルによる構造判定）
    static bool IsValidEucJp(const std::string& bytes);

    // EUC-JP が利用できるか（内蔵テーブルのため常に true）
    static bool IsEucJpAvailable();
    // EUC-JP のコードページ番号（参考値。kernel32 には存在しない）
    static unsigned int EucJpCodePage();

    // 半角カタカナの個数（CP932 / EUC-JP の自動判定ヒューリスティック用）
    static int CountHalfwidthKatakana(const std::wstring& s);
};

} // namespace ms

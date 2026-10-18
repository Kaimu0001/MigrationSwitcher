// =============================================================================
// EucJpTable.h
// EUC-JP 変換テーブル（OS 非依存）
// Windows の kernel32 には EUC-JP のコードページが存在しない。
// 51932 は IsValidCodePage が 0 を返し、MultiByteToWideChar も
// ERROR_INVALID_PARAMETER (87) で失敗する（51932 は MLang 側のコードページ）。
// そのため JIS X 0208 / JIS X 0212 の対応表を内蔵し、OS に依存せず判定・変換する。
// =============================================================================
#pragma once

#include <string>

namespace ms {
namespace eucjp {

// JIS X 0208 の 2 バイト（0xA1A1-0xFEFE）を Unicode へ。0 は未定義。
unsigned short Jis0208ToUnicode(unsigned char b1, unsigned char b2);
// JIS X 0212 の 2 バイト（EUC-JP では先頭に 0x8F が付く）を Unicode へ。0 は未定義。
unsigned short Jis0212ToUnicode(unsigned char b1, unsigned char b2);
// Unicode を EUC-JP のバイト列（2 または 3 バイト）へ追加する。変換不可なら false。
bool UnicodeToEucJp(unsigned short u, std::string& out);
// 半角カタカナ（U+FF61-U+FF9F）を EUC-JP の 2 バイト（0x8E, 0xA1-0xDF）へ追加する。
bool HalfwidthKatakanaToEucJp(unsigned short u, std::string& out);
// 構造的に妥当な EUC-JP バイト列か（ASCII / 半角カタカナ / JIS X 0208 / JIS X 0212）
bool IsValidBytes(const std::string& bytes);
// EUC-JP バイト列を UTF-16 へ。不正なバイトは U+FFFD にする。
std::wstring Decode(const std::string& bytes);
// UTF-16 を EUC-JP バイト列へ。変換できない文字があると false（out は空になる）。
bool Encode(const std::wstring& text, std::string& out);

} // namespace eucjp
} // namespace ms

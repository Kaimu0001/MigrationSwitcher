// =============================================================================
// EncodingDetector.cpp
// =============================================================================
#include "EncodingDetector.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

const wchar_t* EncodingDetector::EncodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::UTF8:     return L"UTF-8";
    case TextEncoding::UTF8BOM:  return L"UTF-8 (BOM)";
    case TextEncoding::ShiftJIS: return L"Shift_JIS";
    case TextEncoding::UTF16LE:  return L"UTF-16 LE";
    default:                     return L"Unknown";
    }
}

bool EncodingDetector::IsValidUtf8(const std::string& bytes)
{
    if (bytes.empty()) return true;
    int r = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  bytes.data(), (int)bytes.size(), nullptr, 0);
    return r > 0;
}

bool EncodingDetector::IsValidShiftJis(const std::string& bytes)
{
    if (bytes.empty()) return true;
    int r = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
                                  bytes.data(), (int)bytes.size(), nullptr, 0);
    return r > 0;
}

TextEncoding EncodingDetector::Detect(const std::string& raw, size_t& bom_length)
{
    bom_length = 0;
    if (raw.size() >= 3 &&
        (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB &&
        (unsigned char)raw[2] == 0xBF) {
        bom_length = 3;
        return TextEncoding::UTF8BOM;
    }
    if (raw.size() >= 2 &&
        (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {
        bom_length = 2;
        return TextEncoding::UTF16LE;
    }
    // BOM なし: まず UTF-8 として厳密判定
    if (IsValidUtf8(raw)) return TextEncoding::UTF8;
    // 次に Shift_JIS(CP932)
    if (IsValidShiftJis(raw)) return TextEncoding::ShiftJIS;
    return TextEncoding::Unknown;
}

bool EncodingDetector::Decode(const std::string& raw,
                              const std::wstring& force_mode,
                              std::wstring& out_text,
                              TextEncoding& out_encoding)
{
    size_t bom = 0;
    TextEncoding detected = Detect(raw, bom);

    std::wstring mode = util::ToUpper(force_mode);
    if (mode.empty()) mode = L"AUTO";

    if (mode == L"UTF8" || mode == L"UTF-8") {
        out_encoding = TextEncoding::UTF8;
        out_text = util::MultiByteToWide(raw, CP_UTF8);
        return !out_text.empty() || raw.empty();
    }
    if (mode == L"UTF8BOM") {
        out_encoding = TextEncoding::UTF8BOM;
        size_t skip = (detected == TextEncoding::UTF8BOM) ? 3 : 0;
        out_text = util::MultiByteToWide(raw.substr(skip), CP_UTF8);
        return true;
    }
    if (mode == L"SJIS" || mode == L"SHIFT_JIS" || mode == L"CP932") {
        out_encoding = TextEncoding::ShiftJIS;
        out_text = util::MultiByteToWide(raw, 932);
        return true;
    }

    // ---- auto ----
    switch (detected) {
    case TextEncoding::UTF8BOM:
        out_encoding = TextEncoding::UTF8BOM;
        out_text = util::MultiByteToWide(raw.substr(3), CP_UTF8);
        return true;
    case TextEncoding::UTF8:
        out_encoding = TextEncoding::UTF8;
        out_text = util::MultiByteToWide(raw, CP_UTF8);
        return true;
    case TextEncoding::UTF16LE: {
        out_encoding = TextEncoding::UTF16LE;
        const char* p = raw.data() + 2;
        size_t chars = (raw.size() - 2) / 2;
        out_text.assign((const wchar_t*)p, chars);
        return true;
    }
    case TextEncoding::ShiftJIS:
        out_encoding = TextEncoding::ShiftJIS;
        out_text = util::MultiByteToWide(raw, 932);
        return true;
    default:
        break;
    }
    // 判定不能: そのまま UTF-8 として読む（文字化けは呼び出し側で警告）
    out_encoding = TextEncoding::Unknown;
    out_text = util::MultiByteToWide(raw, CP_ACP);
    return false;
}

bool EncodingDetector::Encode(const std::wstring& text,
                              TextEncoding encoding,
                              bool with_bom,
                              std::string& out_bytes)
{
    out_bytes.clear();
    switch (encoding) {
    case TextEncoding::UTF16LE: {
        if (with_bom) {
            out_bytes.push_back((char)0xFF);
            out_bytes.push_back((char)0xFE);
        }
        out_bytes.append((const char*)text.data(), text.size() * 2);
        return true;
    }
    case TextEncoding::ShiftJIS: {
        if (text.empty()) return true;
        int need = ::WideCharToMultiByte(932, 0, text.data(), (int)text.size(),
                                         nullptr, 0, nullptr, nullptr);
        if (need <= 0) return false;
        std::string tmp((size_t)need, '\0');
        ::WideCharToMultiByte(932, 0, text.data(), (int)text.size(),
                              &tmp[0], need, nullptr, nullptr);
        out_bytes = tmp;
        return true;
    }
    case TextEncoding::UTF8:
    case TextEncoding::UTF8BOM:
    case TextEncoding::Unknown:
    default: {
        std::string tmp = util::WideToUtf8(text);
        if (with_bom || encoding == TextEncoding::UTF8BOM) {
            out_bytes.push_back((char)0xEF);
            out_bytes.push_back((char)0xBB);
            out_bytes.push_back((char)0xBF);
        }
        out_bytes += tmp;
        return true;
    }
    }
}

} // namespace ms

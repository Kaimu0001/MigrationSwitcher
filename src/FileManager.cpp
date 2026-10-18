// =============================================================================
// FileManager.cpp
// =============================================================================
#include "FileManager.h"
#include <cstdio>
#include "EncodingDetector.h"
#include "Utils.h"

#include <windows.h>

namespace ms {

std::wstring FileManager::LastErrorMessage(unsigned long err)
{
    LPWSTR buf = nullptr;
    DWORD n = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr, err, 0, (LPWSTR)&buf, 0, nullptr);
    std::wstring msg;
    if (n && buf) {
        msg.assign(buf, n);
        while (!msg.empty() && (msg[msg.size() - 1] == L'\r' || msg[msg.size() - 1] == L'\n'))
            msg.erase(msg.size() - 1);
    }
    if (buf) ::LocalFree(buf);
    if (msg.empty()) {
        wchar_t tmp[64];
        swprintf(tmp, 64, L"エラーコード 0x%08lX", err);
        msg = tmp;
    }
    return msg;
}

bool FileManager::IsReadOnlyAttribute(const std::wstring& path)
{
    DWORD attr = ::GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_READONLY) != 0;
}

FileInfo FileManager::Inspect(const std::wstring& path, const std::wstring& encoding_mode)
{
    FileInfo info;
    if (path.empty()) return info;

    DWORD attr = ::GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return info;

    info.exists = true;
    info.is_directory = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    info.read_only_attribute = (attr & FILE_ATTRIBUTE_READONLY) != 0;
    if (info.is_directory) return info;

    // --- 読み取り可否 ---
    HANDLE hr = ::CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hr != INVALID_HANDLE_VALUE) {
        info.readable = true;
        LARGE_INTEGER sz;
        sz.QuadPart = 0;
        ::GetFileSizeEx(hr, &sz);
        info.size = (unsigned long long)sz.QuadPart;

        std::string bytes;
        if (sz.QuadPart > 0 && sz.QuadPart < (LONGLONG)(256 * 1024 * 1024)) {
            bytes.resize((size_t)sz.QuadPart);
            DWORD read = 0;
            ::ReadFile(hr, &bytes[0], (DWORD)bytes.size(), &read, nullptr);
            bytes.resize(read);
        }
        ::CloseHandle(hr);

        // 文字コード判定
        size_t bom = 0;
        TextEncoding enc = EncodingDetector::Detect(bytes, bom);
        std::wstring mode = util::ToUpper(encoding_mode);
        if (mode == L"UTF8") enc = TextEncoding::UTF8;
        else if (mode == L"UTF8BOM") enc = TextEncoding::UTF8BOM;
        else if (mode == L"SJIS") enc = TextEncoding::ShiftJIS;
        info.encoding = enc;
        info.has_bom = (bom > 0);

        std::wstring text;
        TextEncoding real = enc;
        EncodingDetector::Decode(bytes, encoding_mode, text, real);
        info.encoding = real;
        info.newline = util::DetectNewlineKind(text);
        info.newline_string = util::NewlineString(info.newline);
    }

    // --- 書き込み可否 ---
    HANDLE hw = ::CreateFileW(path.c_str(), GENERIC_WRITE,
                              FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hw != INVALID_HANDLE_VALUE) {
        info.writable = true;
        ::CloseHandle(hw);
    } else {
        info.writable = false;
        // 他プロセスが排他ロックしている場合の判定
        HANDLE hx = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                  0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hx == INVALID_HANDLE_VALUE) {
            DWORD e = ::GetLastError();
            if (e == ERROR_SHARING_VIOLATION) info.locked = true;
        } else {
            ::CloseHandle(hx);
        }
    }

    return info;
}

bool FileManager::ReadAllBytes(const std::wstring& path, std::string& out_bytes)
{
    out_bytes.clear();
    HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER sz;
    sz.QuadPart = 0;
    if (!::GetFileSizeEx(h, &sz)) { ::CloseHandle(h); return false; }
    if (sz.QuadPart > 0) {
        out_bytes.resize((size_t)sz.QuadPart);
        DWORD total = 0;
        while (total < (DWORD)out_bytes.size()) {
            DWORD read = 0;
            if (!::ReadFile(h, &out_bytes[total], (DWORD)out_bytes.size() - total, &read, nullptr)) {
                ::CloseHandle(h);
                return false;
            }
            if (read == 0) break;
            total += read;
        }
        out_bytes.resize(total);
    }
    ::CloseHandle(h);
    return true;
}

bool FileManager::ReadText(const std::wstring& path,
                           const std::wstring& encoding_mode,
                           std::wstring& out_text,
                           FileInfo& out_info)
{
    out_text.clear();
    std::string bytes;
    if (!ReadAllBytes(path, bytes)) return false;

    out_info = Inspect(path, encoding_mode);
    TextEncoding enc = TextEncoding::Unknown;
    if (!EncodingDetector::Decode(bytes, encoding_mode, out_text, enc)) {
        out_info.encoding = TextEncoding::Unknown;
    } else {
        out_info.encoding = enc;
    }
    out_info.has_bom = (enc == TextEncoding::UTF8BOM || enc == TextEncoding::UTF16LE);
    out_info.newline = util::DetectNewlineKind(out_text);
    out_info.newline_string = util::NewlineString(out_info.newline);

    // 6.5.3 CRLF / CR を LF へ内部正規化
    out_text = util::NormalizeNewlines(out_text);
    return true;
}

// 19.1 一時ファイルを対象ファイルへ置換する。
// 一時ファイルはモジュール配下の temp/ に作成するため、対象ファイルが別ドライブに
// あると単純な移動は ERROR_NOT_SAME_DEVICE で失敗する。
// MOVEFILE_COPY_ALLOWED でボリュームをまたぐ移動を許可し、それでも失敗する場合は
// コピー + 一時ファイル削除へフォールバックする。
static bool MoveReplace(const std::wstring& from, const std::wstring& to)
{
    if (::MoveFileExW(from.c_str(), to.c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH |
                      MOVEFILE_COPY_ALLOWED)) {
        return true;
    }

    std::wstring err;
    if (!FileManager::CopyFileTo(from, to, err)) return false;
    FileManager::RemoveFile(from);
    return true;
}

bool FileManager::WriteTextAtomic(const std::wstring& path,
                                  const std::wstring& text_lf_normalized,
                                  TextEncoding encoding,
                                  bool with_bom,
                                  NewlineKind newline,
                                  std::wstring& out_error)
{
    out_error.clear();

    // 改行コードを元の形式へ戻す
    std::wstring final_text = util::DenormalizeNewlines(
        text_lf_normalized, util::NewlineString(newline));

    std::string bytes;
    if (!EncodingDetector::Encode(final_text, encoding, with_bom, bytes)) {
        out_error = L"文字コード変換に失敗しました。";
        return false;
    }

    std::wstring dir = util::GetDirectoryOf(path);
    if (dir.empty()) dir = util::ModuleDirectory();
    util::EnsureDirectory(dir);

    // 19.1 一時ファイルへ書き出してから置換する
    std::wstring tmp = MakeTempFilePath(util::GetFileNameOf(path), L".tmp");
    HANDLE h = ::CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        out_error = L"一時ファイルを作成できません: " + LastErrorMessage(::GetLastError());
        return false;
    }
    DWORD written = 0;
    if (!bytes.empty()) {
        if (!::WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr) ||
            written != (DWORD)bytes.size()) {
            out_error = L"一時ファイルへの書き込みに失敗しました: " + LastErrorMessage(::GetLastError());
            ::CloseHandle(h);
            ::DeleteFileW(tmp.c_str());
            return false;
        }
    }
    ::FlushFileBuffers(h);
    ::CloseHandle(h);

    // 書き込み後サイズ確認（19.1）
    HANDLE hv = ::CreateFileW(tmp.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hv == INVALID_HANDLE_VALUE) {
        out_error = L"書き込み後の検証に失敗しました。";
        ::DeleteFileW(tmp.c_str());
        return false;
    }
    LARGE_INTEGER sz;
    sz.QuadPart = 0;
    ::GetFileSizeEx(hv, &sz);
    ::CloseHandle(hv);
    if ((unsigned long long)sz.QuadPart != (unsigned long long)bytes.size()) {
        out_error = L"書き込みサイズが一致しません（破損の可能性）。";
        ::DeleteFileW(tmp.c_str());
        return false;
    }

    // 読み取り専用属性を一時的に解除
    DWORD attr = ::GetFileAttributesW(path.c_str());
    bool was_readonly = (attr != INVALID_FILE_ATTRIBUTES) &&
                        ((attr & FILE_ATTRIBUTE_READONLY) != 0);
    if (was_readonly) {
        ::SetFileAttributesW(path.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
    }

    if (!MoveReplace(tmp, path)) {
        out_error = L"対象ファイルの置換に失敗しました: " + LastErrorMessage(::GetLastError());
        ::DeleteFileW(tmp.c_str());
        if (was_readonly && attr != INVALID_FILE_ATTRIBUTES)
            ::SetFileAttributesW(path.c_str(), attr);
        return false;
    }
    if (was_readonly && attr != INVALID_FILE_ATTRIBUTES)
        ::SetFileAttributesW(path.c_str(), attr);

    return true;
}

bool FileManager::CopyFileTo(const std::wstring& from, const std::wstring& to,
                           std::wstring& out_error)
{
    out_error.clear();
    if (!::CopyFileW(from.c_str(), to.c_str(), FALSE)) {
        out_error = LastErrorMessage(::GetLastError());
        return false;
    }
    return true;
}

std::wstring FileManager::MakeTempFilePath(const std::wstring& base_name,
                                           const std::wstring& ext)
{
    std::wstring temp_dir = util::JoinPath(util::ModuleDirectory(), L"temp");
    util::EnsureDirectory(temp_dir);
    std::wstring stem = util::GetBaseNameNoExt(base_name);
    if (stem.empty()) stem = L"work";
    std::wstring path = util::JoinPath(
        temp_dir, stem + L"_" + util::TimestampFileName() + ext);
    // 衝突回避
    int n = 0;
    while (util::PathExists(path)) {
        wchar_t suffix[16];
        swprintf(suffix, 16, L"_%d", ++n);
        path = util::JoinPath(temp_dir, stem + L"_" + util::TimestampFileName() + suffix + ext);
    }
    return path;
}

bool FileManager::RemoveFile(const std::wstring& path)
{
    if (path.empty()) return false;
    return ::DeleteFileW(path.c_str()) != 0;
}

} // namespace ms

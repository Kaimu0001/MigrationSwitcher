// =============================================================================
// FolderScanner.cpp
// =============================================================================
#include "FolderScanner.h"
#include "Utils.h"

#include <windows.h>

#include <algorithm>

namespace ms {

const int FolderScanner::kMaxFiles = 20000;
const unsigned long long FolderScanner::kMaxFileSize = 8ull * 1024 * 1024;

std::vector<std::wstring> FolderScanner::DefaultExtensions()
{
    // 設計書 17.2.1 の初期チェック状態（.c .cpp .h が ON、.hpp .cs .java が OFF）
    std::vector<std::wstring> v;
    v.push_back(L".c");
    v.push_back(L".cpp");
    v.push_back(L".h");
    return v;
}

std::vector<std::wstring> FolderScanner::CandidateExtensions()
{
    std::vector<std::wstring> v;
    v.push_back(L".c");
    v.push_back(L".cpp");
    v.push_back(L".h");
    v.push_back(L".hpp");
    v.push_back(L".cs");
    v.push_back(L".java");
    return v;
}

bool FolderScanner::MatchesExtension(const std::wstring& path,
                                     const std::vector<std::wstring>& extensions)
{
    std::wstring ext = util::ToUpper(util::GetExtensionOf(path));
    if (ext.empty()) return false;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (ext == util::ToUpper(extensions[i])) return true;
    }
    return false;
}

std::wstring FolderScanner::JoinExtensions(const std::vector<std::wstring>& extensions)
{
    std::wstring out;
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (i) out += L",";
        std::wstring e = extensions[i];
        if (!e.empty() && e[0] == L'.') e = e.substr(1);
        out += e;
    }
    return out;
}

std::vector<std::wstring> FolderScanner::SplitExtensions(const std::wstring& text)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (size_t i = 0; i <= text.size(); ++i) {
        wchar_t c = (i < text.size()) ? text[i] : L',';
        if (c == L',' || c == L';' || c == L' ') {
            std::wstring t = util::Trim(cur);
            if (!t.empty()) {
                if (t[0] != L'.') t = L"." + t;
                out.push_back(t);
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

void FolderScanner::ScanRecursive(const std::wstring& dir,
                                  const std::vector<std::wstring>& extensions,
                                  bool include_subdirs,
                                  ScanResult& out)
{
    if (out.truncated) return;

    ++out.scanned_directories;

    std::wstring pattern = util::JoinPath(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = ::FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = ::GetLastError();
        if (e != ERROR_FILE_NOT_FOUND) {
            out.errors.push_back(L"フォルダを走査できません: " + dir + L"（" +
                                 std::to_wstring((long long)e) + L"）");
        }
        return;
    }

    std::vector<std::wstring> subdirs;

    do {
        if (out.truncated) break;
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;

        std::wstring full = util::JoinPath(dir, name);

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            // シンボリックリンク／ジャンクションは無限ループ防止のため辿らない
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (include_subdirs) subdirs.push_back(full);
            continue;
        }

        if (!MatchesExtension(name, extensions)) continue;

        ULARGE_INTEGER sz;
        sz.LowPart  = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;
        if (sz.QuadPart > kMaxFileSize) {
            out.skipped.push_back(full + L"（サイズ過大: " +
                                  std::to_wstring((long long)sz.QuadPart) + L" bytes）");
            continue;
        }

        out.files.push_back(full);
        if ((int)out.files.size() >= kMaxFiles) {
            out.truncated = true;
            break;
        }
    } while (::FindNextFileW(h, &fd));

    ::FindClose(h);

    for (size_t i = 0; i < subdirs.size() && !out.truncated; ++i)
        ScanRecursive(subdirs[i], extensions, include_subdirs, out);
}

void FolderScanner::Scan(const std::wstring& folder,
                         const std::vector<std::wstring>& extensions,
                         bool include_subdirs,
                         ScanResult& out)
{
    out = ScanResult();
    if (folder.empty()) {
        out.errors.push_back(L"対象フォルダが指定されていません。");
        return;
    }
    if (!util::PathExists(folder)) {
        out.errors.push_back(L"指定されたフォルダが存在しません: " + folder);
        return;
    }

    DWORD attr = ::GetFileAttributesW(folder.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES ||
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        out.errors.push_back(L"指定されたパスはフォルダではありません: " + folder);
        return;
    }

    std::vector<std::wstring> ext = extensions.empty() ? DefaultExtensions() : extensions;
    ScanRecursive(folder, ext, include_subdirs, out);

    // 走査順を安定させる（フォルダ走査順は OS 依存のため）
    std::sort(out.files.begin(), out.files.end());
}

} // namespace ms

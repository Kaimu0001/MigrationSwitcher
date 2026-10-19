// =============================================================================
// HistoryManager.cpp
// =============================================================================
#include "HistoryManager.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

#include <algorithm>

namespace ms {

static std::wstring Join(const std::wstring& a, const std::wstring& b)
{
    return util::JoinPath(a, b);
}

static std::wstring ReadIniString(const std::wstring& ini, const wchar_t* sec,
                                  const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[2048];
    ::GetPrivateProfileStringW(sec, key, def, buf, 2048, ini.c_str());
    return std::wstring(buf);
}

static int ReadIniInt(const std::wstring& ini, const wchar_t* sec,
                      const wchar_t* key, int def)
{
    return (int)::GetPrivateProfileIntW(sec, key, def, ini.c_str());
}

static std::wstring MakeRunId()
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    wchar_t buf[64];
    swprintf(buf, 64, L"run_%04d%02d%02d_%02d%02d%02d",
             (int)st.wYear, (int)st.wMonth, (int)st.wDay,
             (int)st.wHour, (int)st.wMinute, (int)st.wSecond);
    std::wstring id(buf);
    // 同一秒に複数回実行された場合は連番を付ける
    std::wstring root = util::JoinPath(util::ModuleDirectory(), L"__ms_run_probe");
    (void)root;
    return id;
}

std::wstring HistoryManager::RootFor(const std::wstring& base_dir)
{
    std::wstring base = base_dir;
    if (base.empty()) base = util::ModuleDirectory();
    return util::JoinPath(base, L"undobak");
}

std::wstring HistoryManager::SidecarDirectory(const std::wstring& base_dir)
{
    return util::JoinPath(RootFor(base_dir), L"sidecars");
}

bool HistoryManager::Record(const std::wstring& base_dir,
                            const std::wstring& operation,
                            const std::wstring& mode,
                            int replaced,
                            const std::vector<HistoryFileEntry>& src,
                            HistoryRunInfo& out_run,
                            std::wstring& out_error)
{
    out_error.clear();
    if (src.empty()) { out_error = L"記録するファイルがありません。"; return false; }

    std::wstring root = RootFor(base_dir);
    std::wstring runs = Join(root, L"runs");
    if (!util::EnsureDirectory(runs)) {
        out_error = L"履歴フォルダを作成できません: " + runs;
        return false;
    }

    std::wstring base_id = MakeRunId();
    std::wstring id = base_id;
    for (int n = 2; util::PathExists(Join(runs, id)); ++n) {
        wchar_t suf[16];
        swprintf(suf, 16, L"_%03d", n);
        id = base_id + suf;
    }

    std::wstring dir = Join(runs, id);
    std::wstring fdir = Join(dir, L"files");
    if (!util::EnsureDirectory(fdir)) {
        out_error = L"履歴フォルダを作成できません: " + fdir;
        return false;
    }

    std::wstring tsv;
    int idx = 0;
    for (size_t i = 0; i < src.size(); ++i) {
        const HistoryFileEntry& e = src[i];
        if (e.path.empty()) continue;
        if (!e.has_before && !e.has_after) continue;

        wchar_t num[16];
        swprintf(num, 16, L"%04d", ++idx);
        std::wstring base = util::GetFileNameOf(e.path);
        std::wstring bp = Join(fdir, std::wstring(num) + L"_" + base + L".before");
        std::wstring ap = Join(fdir, std::wstring(num) + L"_" + base + L".after");

        std::wstring err;
        bool ok_b = true, ok_a = true;
        if (e.has_before && util::PathExists(e.before)) {
            if (!FileManager::CopyFileTo(e.before, bp, err)) ok_b = false;
        } else {
            ok_b = false;
        }
        if (e.has_after && util::PathExists(e.after)) {
            if (!FileManager::CopyFileTo(e.after, ap, err)) ok_a = false;
        } else {
            ok_a = false;
        }
        if (!ok_b && !ok_a) { --idx; continue; }

        tsv += std::wstring(num) + L"\t" + e.path + L"\t" +
               (ok_b ? std::wstring(L"files\\") + std::wstring(num) + L"_" + base + L".before" : L"") + L"\t" +
               (ok_a ? std::wstring(L"files\\") + std::wstring(num) + L"_" + base + L".after" : L"") + L"\r\n";
    }

    if (idx == 0) {
        out_error = L"スナップショットを作成できませんでした。";
        return false;
    }

    std::wstring meta = Join(dir, L"meta.ini");
    ::WritePrivateProfileStringW(L"Run", L"Time", util::NowDateTime().c_str(), meta.c_str());
    ::WritePrivateProfileStringW(L"Run", L"Operation", operation.c_str(), meta.c_str());
    ::WritePrivateProfileStringW(L"Run", L"Mode", mode.c_str(), meta.c_str());
    ::WritePrivateProfileStringW(L"Run", L"Files", std::to_wstring(idx).c_str(), meta.c_str());
    ::WritePrivateProfileStringW(L"Run", L"Replaced", std::to_wstring(replaced).c_str(), meta.c_str());

    std::wstring tsv_path = Join(dir, L"files.tsv");
    std::wstring werr;
    FileManager::WriteTextAtomic(tsv_path, tsv, TextEncoding::UTF8, true,
                                 NewlineKind::CRLF, werr);

    out_run.id = id;
    out_run.time = util::NowDateTime();
    out_run.operation = operation;
    out_run.mode = mode;
    out_run.files = idx;
    out_run.replaced = replaced;
    return true;
}

std::vector<HistoryRunInfo> HistoryManager::ListRuns(const std::wstring& base_dir)
{
    std::vector<HistoryRunInfo> out;
    std::wstring runs = Join(RootFor(base_dir), L"runs");

    WIN32_FIND_DATAW fd;
    HANDLE h = ::FindFirstFileW(Join(runs, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;

    do {
        if (lstrcmpW(fd.cFileName, L".") == 0 || lstrcmpW(fd.cFileName, L"..") == 0)
            continue;
        HistoryRunInfo r;
        r.id = fd.cFileName;
        std::wstring meta = Join(runs, r.id);
        meta = Join(meta, L"meta.ini");
        if (!util::PathExists(meta)) continue;
        r.time = ReadIniString(meta, L"Run", L"Time", L"");
        r.operation = ReadIniString(meta, L"Run", L"Operation", L"");
        r.mode = ReadIniString(meta, L"Run", L"Mode", L"");
        r.files = ReadIniInt(meta, L"Run", L"Files", 0);
        r.replaced = ReadIniInt(meta, L"Run", L"Replaced", 0);
        out.push_back(r);
    } while (::FindNextFileW(h, &fd));
    ::FindClose(h);

    std::sort(out.begin(), out.end(), [](const HistoryRunInfo& a, const HistoryRunInfo& b) {
        return a.id > b.id;   // 名前は時刻順なので降順 = 新しい順
    });
    return out;
}

std::vector<HistoryFileEntry> HistoryManager::ListFiles(const std::wstring& base_dir,
                                                        const std::wstring& run_id)
{
    std::vector<HistoryFileEntry> out;
    if (run_id.empty()) return out;
    std::wstring dir = Join(Join(RootFor(base_dir), L"runs"), run_id);
    std::wstring tsv_path = Join(dir, L"files.tsv");
    if (!util::PathExists(tsv_path)) return out;

    std::wstring text;
    std::wstring err;
    if (!LoadText(tsv_path, text, err)) return out;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos) ? text.substr(pos) : text.substr(pos, eol - pos);
        pos = (eol == std::wstring::npos) ? text.size() : eol + 1;
        if (!line.empty() && line[line.size() - 1] == L'\r') line.erase(line.size() - 1);
        if (line.empty()) continue;

        std::vector<std::wstring> f;
        size_t p = 0;
        while (true) {
            size_t t = line.find(L'\t', p);
            if (t == std::wstring::npos) { f.push_back(line.substr(p)); break; }
            f.push_back(line.substr(p, t - p));
            p = t + 1;
        }
        if (f.size() < 4) continue;

        HistoryFileEntry e;
        e.path = f[1];
        if (!f[2].empty()) {
            e.before = Join(dir, f[2]);
            e.has_before = util::PathExists(e.before);
        }
        if (!f[3].empty()) {
            e.after = Join(dir, f[3]);
            e.has_after = util::PathExists(e.after);
        }
        out.push_back(e);
    }
    return out;
}

bool HistoryManager::LoadText(const std::wstring& path, std::wstring& out, std::wstring& out_error)
{
    out.clear();
    out_error.clear();
    if (!util::PathExists(path)) { out_error = L"ファイルがありません: " + path; return false; }
    std::wstring text;
    FileInfo info;
    if (!FileManager::ReadText(path, L"auto", text, info)) {
        out_error = L"読み取れません: " + path;
        return false;
    }
    out = text;
    return true;
}

std::wstring HistoryManager::BuildDiffText(const std::wstring& before,
                                           const std::wstring& after,
                                           int max_lines)
{
    std::vector<std::wstring> lb, la;
    {
        std::wstring cur;
        for (size_t i = 0; i < before.size(); ++i) {
            if (before[i] == L'\n') { lb.push_back(cur); cur.clear(); } else cur.push_back(before[i]);
        }
        lb.push_back(cur);
        cur.clear();
        for (size_t i = 0; i < after.size(); ++i) {
            if (after[i] == L'\n') { la.push_back(cur); cur.clear(); } else cur.push_back(after[i]);
        }
        la.push_back(cur);
    }

    std::wstring out;
    size_t n = lb.size(), m = la.size();
    if (n > 4000 || m > 4000) {
        out += L"（行数が多いため差分を省略: 変更前 " + std::to_wstring((int)n) +
               L" 行 / 変更後 " + std::to_wstring((int)m) + L" 行）\n";
        return out;
    }

    // 単純な LCS（行単位）
    std::vector<int> prev(m + 1, 0), cur(m + 1, 0);
    std::vector<std::vector<int> > dp(n + 1, std::vector<int>(m + 1, 0));
    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (lb[i - 1] == la[j - 1]) dp[i][j] = dp[i - 1][j - 1] + 1;
            else dp[i][j] = (dp[i - 1][j] >= dp[i][j - 1]) ? dp[i - 1][j] : dp[i][j - 1];
        }
    }
    (void)prev; (void)cur;

    std::vector<std::wstring> lines;
    size_t i = n, j = m;
    while (i > 0 && j > 0) {
        if (lb[i - 1] == la[j - 1]) {
            lines.push_back(L"  " + lb[i - 1]);
            --i; --j;
        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            lines.push_back(L"- " + lb[i - 1]);
            --i;
        } else {
            lines.push_back(L"+ " + la[j - 1]);
            --j;
        }
    }
    while (i > 0) { lines.push_back(L"- " + lb[i - 1]); --i; }
    while (j > 0) { lines.push_back(L"+ " + la[j - 1]); --j; }

    int shown = 0;
    for (size_t k = lines.size(); k > 0; --k) {
        if (max_lines > 0 && shown >= max_lines) {
            out += L"...（省略）\n";
            break;
        }
        out += lines[k - 1] + L"\n";
        ++shown;
    }
    return out;
}

std::wstring HistoryManager::RunLabel(const HistoryRunInfo& r)
{
    std::wstring s;
    std::wstring time = r.time;
    if (time.size() >= 16) time = time.substr(5, 11);   // MM-DD HH:MM
    s += time;
    if (!r.mode.empty()) s += L" [" + r.mode + L"]";
    if (r.replaced > 0) s += L" " + std::to_wstring(r.replaced) + L" 件";
    s += L" " + std::to_wstring(r.files) + L" ファイル";
    return s;
}

} // namespace ms

// =============================================================================
// SwitchHeaderManager.cpp
// =============================================================================
#include "SwitchHeaderManager.h"
#include "EncodingDetector.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

#include <algorithm>

namespace ms {

static const wchar_t* kBeginMarker = L"/* ==== CopyMigration Switcher managed section: BEGIN ==== */";
static const wchar_t* kEndMarker   = L"/* ==== CopyMigration Switcher managed section: END ==== */";

// -----------------------------------------------------------------------------
// パス・名前
// -----------------------------------------------------------------------------
std::wstring SwitchHeaderManager::DefaultHeaderName()
{
    return L"migration_switch.h";
}

std::wstring SwitchHeaderManager::GuardFromName(const std::wstring& header_name)
{
    std::wstring g = util::ToUpper(util::GetFileNameOf(header_name));
    std::wstring out;
    for (size_t i = 0; i < g.size(); ++i) {
        wchar_t c = g[i];
        bool ok = (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9');
        out += ok ? c : L'_';
    }
    if (out.empty()) out = L"MIGRATION_SWITCH_H";
    if (out[0] >= L'0' && out[0] <= L'9') out = L"_" + out;
    return out;
}

bool SwitchHeaderManager::ValidateHeaderName(const std::wstring& header_name, std::wstring& out_error)
{
    out_error.clear();
    if (util::Trim(header_name).empty()) {
        out_error = L"スイッチ定義ヘッダのファイル名を入力してください。";
        return false;
    }
    std::wstring name = util::GetFileNameOf(util::Trim(header_name));
    if (name.empty()) {
        out_error = L"スイッチ定義ヘッダのファイル名が不正です。";
        return false;
    }
    static const wchar_t* bad = L"\\/:*?\"<>|";
    for (size_t i = 0; i < name.size(); ++i) {
        if (wcschr(bad, name[i])) {
            out_error = L"ファイル名に使用できない文字が含まれています: " + name;
            return false;
        }
    }
    if (util::ToUpper(util::GetExtensionOf(name)) != L".H") {
        out_error = L"スイッチ定義ヘッダの拡張子は .h にしてください: " + name;
        return false;
    }
    return true;
}

std::wstring SwitchHeaderManager::ResolveHeaderPath(const std::wstring& base_dir,
                                                    const std::wstring& header_dir,
                                                    const std::wstring& header_name)
{
    std::wstring name = util::GetFileNameOf(util::Trim(header_name));
    if (name.empty()) name = DefaultHeaderName();

    std::wstring dir = util::Trim(header_dir);
    if (dir.empty()) dir = base_dir;
    if (dir.empty()) dir = util::ModuleDirectory();
    return util::JoinPath(util::MakeAbsolute(dir), name);
}

std::wstring SwitchHeaderManager::MakeIncludePath(const std::wstring& source_path,
                                                  const std::wstring& header_path)
{
    std::wstring name = util::GetFileNameOf(header_path);
    std::wstring sdir = util::ToUpper(util::MakeAbsolute(util::GetDirectoryOf(source_path)));
    std::wstring hdir = util::ToUpper(util::MakeAbsolute(util::GetDirectoryOf(header_path)));

    if (sdir == hdir) return name;

    // 相対パスを組み立てる（同一ドライブの場合のみ）
    if (sdir.size() >= 2 && hdir.size() >= 2 && sdir[1] == L':' && hdir[1] == L':') {
        if (sdir[0] != hdir[0]) {
            // ドライブが異なる場合は絶対パスを使う（/ 区切りへ変換）
            std::wstring abs = util::MakeAbsolute(header_path);
            for (size_t i = 0; i < abs.size(); ++i) if (abs[i] == L'\\') abs[i] = L'/';
            return abs;
        }
        std::vector<std::wstring> sp = util::SplitPath(sdir);
        std::vector<std::wstring> hp = util::SplitPath(hdir);
        size_t common = 0;
        while (common < sp.size() && common < hp.size() &&
               util::ToUpper(sp[common]) == util::ToUpper(hp[common])) ++common;

        std::wstring rel;
        for (size_t i = common; i < sp.size(); ++i) rel += L"../";
        for (size_t i = common; i < hp.size(); ++i) {
            rel += hp[i];
            rel += L"/";
        }
        rel += name;
        if (rel.empty()) rel = name;
        return rel;
    }

    std::wstring abs = util::MakeAbsolute(header_path);
    for (size_t i = 0; i < abs.size(); ++i) if (abs[i] == L'\\') abs[i] = L'/';
    return abs;
}

// -----------------------------------------------------------------------------
// 要約・整形ヘルパ
// -----------------------------------------------------------------------------
std::wstring SwitchHeaderManager::FirstMeaningfulLine(const std::wstring& code, size_t max_len)
{
    std::vector<std::wstring> lines = util::SplitLines(code);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring t = util::Trim(lines[i]);
        if (t.empty()) continue;
        if (t.size() > max_len) t = t.substr(0, max_len) + L"...";
        return t;
    }
    return std::wstring();
}

std::wstring SwitchHeaderManager::MakeSummary(const std::wstring& old_code,
                                              const std::wstring& new_code)
{
    std::wstring o = FirstMeaningfulLine(old_code, 48);
    std::wstring n = FirstMeaningfulLine(new_code, 48);
    if (n.empty()) n = L"(新コード未入力)";
    if (o.empty()) o = L"(旧コード未入力)";
    return o + L" -> " + n;
}

// -----------------------------------------------------------------------------
// 管理領域の生成・解析
// -----------------------------------------------------------------------------
std::wstring SwitchHeaderManager::BuildManagedSection(const std::vector<SwitchEntry>& entries)
{
    std::wstring s;
    s += kBeginMarker;
    s += L"\n";
    s += L"/* この領域は CopyMigration Switcher が自動生成・管理します。 */\n";
    s += L"/* スイッチを有効にするには #define の行のコメント（/ * * /）を外してください。 */\n";
    s += L"/* 定義を有効にすると、対象ソースの #ifdef 側（新コード）が有効になります。 */\n";
    s += L"\n";

    for (size_t i = 0; i < entries.size(); ++i) {
        const SwitchEntry& e = entries[i];

        std::wstring comment = e.comment;
        if (!e.summary.empty()) {
            if (!comment.empty()) comment += L" | ";
            comment += e.summary;
        }
        std::wstring files;
        for (size_t k = 0; k < e.files.size(); ++k) {
            if (k) files += L", ";
            files += e.files[k];
        }
        if (!files.empty()) comment += L"  [" + files + L"]";
        if (!e.updated_at.empty()) comment += L"  " + e.updated_at;

        s += L"/* " + e.name + L" : " + comment + L" */\n";
        if (e.enabled) {
            s += L"#define " + e.name + L"\n";
        } else {
            s += L"/* #define " + e.name + L" */\n";
        }
        s += L"\n";
    }

    s += kEndMarker;
    s += L"\n";
    return s;
}

void SwitchHeaderManager::ParseManagedSection(const std::wstring& section,
                                              std::vector<SwitchEntry>& out)
{
    out.clear();
    std::vector<std::wstring> lines = util::SplitLines(section);

    SwitchEntry cur;
    bool has_cur = false;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring t = util::Trim(lines[i]);
        if (t.empty()) continue;
        if (t == kBeginMarker || t == kEndMarker) continue;
        if (t.compare(0, 2, L"/*") != 0 && t.compare(0, 1, L"#") != 0) continue;
        if (t.find(L"この領域は") != std::wstring::npos) continue;
        if (t.find(L"スイッチを有効に") != std::wstring::npos) continue;
        if (t.find(L"定義を有効に") != std::wstring::npos) continue;

        // コメント行（"/* NAME : comment */"）
        if (t.compare(0, 2, L"/*") == 0 && t.find(L"#define") == std::wstring::npos) {
            if (t.size() > 4) {
                std::wstring body = t.substr(2);
                size_t endc = body.rfind(L"*/");
                if (endc != std::wstring::npos) body = body.substr(0, endc);
                body = util::Trim(body);
                size_t sep = body.find(L" : ");
                if (sep != std::wstring::npos) {
                    if (has_cur) out.push_back(cur);
                    cur = SwitchEntry();
                    cur.name = util::Trim(body.substr(0, sep));
                    cur.comment = util::Trim(body.substr(sep + 3));
                    // ファイル一覧を取り出す（末尾の [a, b] を解析）
                    size_t lb = cur.comment.rfind(L'[');
                    size_t rb = cur.comment.rfind(L']');
                    if (lb != std::wstring::npos && rb != std::wstring::npos && rb > lb) {
                        std::wstring fl = cur.comment.substr(lb + 1, rb - lb - 1);
                        std::vector<std::wstring> parts;
                        std::wstring acc;
                        for (size_t k = 0; k <= fl.size(); ++k) {
                            wchar_t c = (k < fl.size()) ? fl[k] : L',';
                            if (c == L',') {
                                std::wstring p = util::Trim(acc);
                                if (!p.empty()) parts.push_back(p);
                                acc.clear();
                            } else acc += c;
                        }
                        cur.files = parts;
                        cur.comment = util::Trim(cur.comment.substr(0, lb));
                    }
                    has_cur = true;
                }
            }
            continue;
        }

        // 定義行（"#define NAME" または "/* #define NAME */"）
        size_t dp = t.find(L"#define");
        if (dp != std::wstring::npos) {
            std::wstring rest = util::Trim(t.substr(dp + 7));
            std::wstring name;
            for (size_t k = 0; k < rest.size(); ++k) {
                wchar_t c = rest[k];
                if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
                    (c >= L'0' && c <= L'9') || c == L'_') name += c;
                else break;
            }
            if (name.empty()) continue;

            bool commented = (t.compare(0, 2, L"/*") == 0);
            if (has_cur && cur.name == name) {
                cur.enabled = !commented;
                out.push_back(cur);
                has_cur = false;
                cur = SwitchEntry();
            } else {
                // コメント行が無い定義も拾う
                if (has_cur) { out.push_back(cur); has_cur = false; }
                SwitchEntry e;
                e.name = name;
                e.enabled = !commented;
                out.push_back(e);
                cur = SwitchEntry();
            }
        }
    }
    if (has_cur) out.push_back(cur);
}

// -----------------------------------------------------------------------------
// 読み込み・書き込み
// -----------------------------------------------------------------------------
bool SwitchHeaderManager::Load(const std::wstring& header_path, SwitchHeaderInfo& out)
{
    out = SwitchHeaderInfo();
    out.path = header_path;
    out.guard = GuardFromName(header_path);

    if (!util::PathExists(header_path)) return true;   // 未作成は正常（exists=false）

    std::wstring text;
    FileInfo info;
    if (!FileManager::ReadText(header_path, L"auto", text, info)) return false;
    out.exists = true;

    size_t b = text.find(kBeginMarker);
    size_t e = text.find(kEndMarker);
    if (b != std::wstring::npos && e != std::wstring::npos && e > b) {
        out.has_managed_section = true;
        out.before_section = text.substr(0, b);
        out.after_section  = text.substr(e + wcslen(kEndMarker));
        if (!out.after_section.empty() && out.after_section[0] == L'\n')
            out.after_section.erase(0, 1);
        std::wstring section = text.substr(b, e - b);
        ParseManagedSection(section, out.entries);
    } else {
        out.has_managed_section = false;
        out.before_section = text;
        out.after_section.clear();
    }
    return true;
}

std::wstring SwitchHeaderManager::BuildFullContent(const SwitchHeaderInfo& info)
{
    std::wstring guard = info.guard.empty() ? GuardFromName(info.path) : info.guard;
    std::wstring managed = BuildManagedSection(info.entries);

    if (info.has_managed_section) {
        std::wstring s = info.before_section;
        if (!s.empty() && s[s.size() - 1] != L'\n') s += L"\n";
        s += managed;
        std::wstring tail = info.after_section;
        if (!tail.empty() && tail[0] == L'\n') tail.erase(0, 1);
        s += tail;
        return s;
    }

    if (info.exists && !info.before_section.empty()) {
        // 既存ファイルに管理領域が無い場合は、先頭のインクルードガード直後に挿入する
        std::vector<std::wstring> lines = util::SplitLines(info.before_section);
        int insert_at = -1;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::wstring t = util::Trim(lines[i]);
            if (t.size() > 8 && t.compare(0, 7, L"#define") == 0) {
                std::wstring body = util::Trim(t.substr(7));
                if (body.find(guard) != std::wstring::npos) { insert_at = (int)i + 1; break; }
            }
        }
        if (insert_at < 0) {
            // 先頭のコメントブロックの後ろ
            insert_at = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                std::wstring t = util::Trim(lines[i]);
                if (t.empty() || t.compare(0, 2, L"/*") == 0 || t.compare(0, 2, L"//") == 0) {
                    insert_at = (int)i + 1;
                    if (t.compare(0, 2, L"/*") == 0 && t.find(L"*/") == std::wstring::npos) {
                        // ブロックコメントの終端まで進める
                        for (size_t k = i + 1; k < lines.size(); ++k) {
                            insert_at = (int)k + 1;
                            if (lines[k].find(L"*/") != std::wstring::npos) { i = k; break; }
                        }
                    }
                } else break;
            }
        }
        std::wstring s;
        for (size_t i = 0; i < lines.size(); ++i) {
            if ((int)i == insert_at) s += managed;
            s += lines[i];
            s += L"\n";
        }
        if (insert_at >= (int)lines.size()) s += managed;
        return s;
    }

    // 新規作成
    std::wstring s;
    s += L"/* ============================================================================\n";
    s += L" * " + util::GetFileNameOf(info.path) + L"\n";
    s += L" * マイグレーション用コンパイルスイッチ定義\n";
    s += L" *\n";
    s += L" * このファイルは CopyMigration Switcher が管理します。\n";
    s += L" * 対象ソースは本ファイルを #include し、#ifdef <スイッチ名> で新旧を切り替えます。\n";
    s += L" * 既定はすべて無効（コメントアウト）です。必要なスイッチだけ有効化してください。\n";
    s += L" * ==========================================================================*/\n";
    s += L"#ifndef " + guard + L"\n";
    s += L"#define " + guard + L"\n";
    s += L"\n";
    s += managed;
    s += L"\n";
    s += L"/* ---- 手動定義領域（この領域はツールが書き換えません） ---- */\n";
    s += L"\n";
    s += L"#endif /* " + guard + L" */\n";
    return s;
}

bool SwitchHeaderManager::Write(const std::wstring& header_path,
                                const SwitchHeaderInfo& info,
                                std::wstring& out_error)
{
    out_error.clear();

    std::wstring content = BuildFullContent(info);

    std::wstring dir = util::GetDirectoryOf(header_path);
    if (!dir.empty() && !util::EnsureDirectory(dir)) {
        out_error = L"スイッチ定義ヘッダの出力先フォルダを作成できません: " + dir;
        return false;
    }

    // 新規は UTF-8、既存は元の文字コード・改行コードを維持する
    TextEncoding enc = TextEncoding::UTF8;
    bool with_bom = false;
    NewlineKind nl = NewlineKind::CRLF;
    if (util::PathExists(header_path)) {
        std::wstring old;
        FileInfo fi;
        if (FileManager::ReadText(header_path, L"auto", old, fi)) {
            enc = fi.encoding;
            with_bom = fi.has_bom;
            nl = fi.newline;
        }
    }
    return FileManager::WriteTextAtomic(header_path, content, enc, with_bom, nl, out_error);
}

// -----------------------------------------------------------------------------
// スイッチの登録・切替・削除
// -----------------------------------------------------------------------------
bool SwitchHeaderManager::RegisterSwitch(const std::wstring& header_path,
                                         const std::wstring& switch_name,
                                         const std::wstring& memo,
                                         const std::wstring& old_code,
                                         const std::wstring& new_code,
                                         const std::vector<std::wstring>& source_files,
                                         SwitchHeaderInfo& out_info,
                                         bool& out_added,
                                         std::wstring& out_error)
{
    out_added = false;
    out_error.clear();

    if (!Load(header_path, out_info)) {
        out_error = L"既存のスイッチ定義ヘッダを読み込めません: " + header_path;
        return false;
    }

    std::wstring now = util::NowDateTime();

    bool found = false;
    for (size_t i = 0; i < out_info.entries.size(); ++i) {
        SwitchEntry& e = out_info.entries[i];
        if (util::ToUpper(e.name) != util::ToUpper(switch_name)) continue;
        found = true;
        if (!memo.empty()) e.comment = memo;
        if (e.summary.empty()) e.summary = MakeSummary(old_code, new_code);
        e.updated_at = now;
        for (size_t f = 0; f < source_files.size(); ++f) {
            std::wstring fname = util::GetFileNameOf(source_files[f]);
            if (fname.empty()) continue;
            bool has_file = false;
            for (size_t k = 0; k < e.files.size(); ++k)
                if (util::ToUpper(e.files[k]) == util::ToUpper(fname)) has_file = true;
            if (has_file) continue;
            if (e.files.size() < 20) e.files.push_back(fname);
            else if (e.files.empty() || e.files[e.files.size() - 1] != L"...") e.files.push_back(L"...");
        }
        break;
    }

    if (!found) {
        SwitchEntry e;
        e.name = switch_name;
        e.comment = memo;
        e.summary = MakeSummary(old_code, new_code);
        e.updated_at = now;
        for (size_t f = 0; f < source_files.size() && e.files.size() < 20; ++f) {
            std::wstring fname = util::GetFileNameOf(source_files[f]);
            if (!fname.empty()) e.files.push_back(fname);
        }
        e.enabled = false;                 // 既定は無効（＝旧コードのまま。安全側）
        out_info.entries.push_back(e);
        out_added = true;
    }

    // 名前順に整列して見通しを良くする
    std::sort(out_info.entries.begin(), out_info.entries.end(),
              [](const SwitchEntry& a, const SwitchEntry& b) { return a.name < b.name; });

    if (!Write(header_path, out_info, out_error)) return false;
    return true;
}

bool SwitchHeaderManager::SetEnabled(const std::wstring& header_path,
                                     const std::wstring& switch_name,
                                     bool enabled,
                                     SwitchHeaderInfo& out_info,
                                     std::wstring& out_error)
{
    out_error.clear();

    if (!Load(header_path, out_info)) {
        out_error = L"スイッチ定義ヘッダを読み込めません: " + header_path;
        return false;
    }
    bool found = false;
    for (size_t i = 0; i < out_info.entries.size(); ++i) {
        if (util::ToUpper(out_info.entries[i].name) != util::ToUpper(switch_name)) continue;
        out_info.entries[i].enabled = enabled;
        out_info.entries[i].updated_at = util::NowDateTime();
        found = true;
        break;
    }
    if (!found) {
        out_error = L"スイッチが見つかりません: " + switch_name;
        return false;
    }
    return Write(header_path, out_info, out_error);
}

bool SwitchHeaderManager::RemoveSwitch(const std::wstring& header_path,
                                       const std::wstring& switch_name,
                                       SwitchHeaderInfo& out_info,
                                       std::wstring& out_error)
{
    out_error.clear();

    if (!Load(header_path, out_info)) {
        out_error = L"スイッチ定義ヘッダを読み込めません: " + header_path;
        return false;
    }
    std::vector<SwitchEntry> keep;
    bool found = false;
    for (size_t i = 0; i < out_info.entries.size(); ++i) {
        if (util::ToUpper(out_info.entries[i].name) == util::ToUpper(switch_name)) {
            found = true;
            continue;
        }
        keep.push_back(out_info.entries[i]);
    }
    if (!found) {
        out_error = L"スイッチが見つかりません: " + switch_name;
        return false;
    }
    out_info.entries = keep;
    return Write(header_path, out_info, out_error);
}

// -----------------------------------------------------------------------------
// ソースへの #include 追加
// -----------------------------------------------------------------------------
bool SwitchHeaderManager::HasInclude(const std::wstring& text, const std::wstring& include_path)
{
    if (include_path.empty()) return true;
    std::wstring base = util::GetFileNameOf(include_path);
    std::vector<std::wstring> lines = util::SplitLines(text);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring t = util::Trim(lines[i]);
        if (t.compare(0, 8, L"#include") != 0) continue;
        if (t.find(base) != std::wstring::npos) return true;
    }
    return false;
}

bool SwitchHeaderManager::EnsureInclude(std::wstring& text,
                                        const std::wstring& include_path,
                                        bool& out_added,
                                        int& out_line,
                                        std::wstring& out_error)
{
    out_added = false;
    out_line = 0;
    out_error.clear();

    if (include_path.empty()) {
        out_error = L"#include するヘッダのパスが空です。";
        return false;
    }
    if (HasInclude(text, include_path)) return true;

    std::vector<std::wstring> lines = util::SplitLines(text);
    bool trailing_newline = (!text.empty() && text[text.size() - 1] == L'\n');

    int insert_at = -1;

    // 1) 最後の #include の直後
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring t = util::Trim(lines[i]);
        if (t.compare(0, 8, L"#include") == 0) insert_at = (int)i + 1;
    }

    // 2) インクルードガード（#ifndef X / #define X）の直後
    if (insert_at < 0) {
        for (size_t i = 0; i + 1 < lines.size(); ++i) {
            std::wstring a = util::Trim(lines[i]);
            std::wstring b = util::Trim(lines[i + 1]);
            if (a.compare(0, 7, L"#ifndef") == 0 && b.compare(0, 7, L"#define") == 0) {
                insert_at = (int)i + 2;
                break;
            }
        }
    }

    // 3) 先頭のコメントブロック・空行の後ろ
    if (insert_at < 0) {
        insert_at = 0;
        bool in_block = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::wstring t = util::Trim(lines[i]);
            if (in_block) {
                insert_at = (int)i + 1;
                if (t.find(L"*/") != std::wstring::npos) in_block = false;
                continue;
            }
            if (t.empty()) { insert_at = (int)i + 1; continue; }
            if (t.compare(0, 2, L"//") == 0) { insert_at = (int)i + 1; continue; }
            if (t.compare(0, 2, L"/*") == 0) {
                insert_at = (int)i + 1;
                if (t.find(L"*/") == std::wstring::npos) in_block = true;
                continue;
            }
            break;
        }
    }

    if (insert_at > (int)lines.size()) insert_at = (int)lines.size();

    std::vector<std::wstring> out;
    out.reserve(lines.size() + 2);
    for (int i = 0; i < insert_at; ++i) out.push_back(lines[(size_t)i]);
    out.push_back(L"#include \"" + include_path + L"\"");

    bool blank_after = false;
    if (insert_at < (int)lines.size() && util::Trim(lines[(size_t)insert_at]).empty())
        blank_after = true;
    if (!blank_after && insert_at < (int)lines.size()) out.push_back(L"");

    for (int i = insert_at; i < (int)lines.size(); ++i) out.push_back(lines[(size_t)i]);

    text = util::JoinLines(out, L"\n");
    if (trailing_newline && (text.empty() || text[text.size() - 1] != L'\n')) text += L"\n";

    out_added = true;
    out_line = insert_at + 1;      // 1 起点
    return true;
}

} // namespace ms

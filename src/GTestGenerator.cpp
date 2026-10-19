// =============================================================================
// GTestGenerator.cpp
// =============================================================================
#include "GTestGenerator.h"
#include "CodeScope.h"
#include "FileManager.h"
#include "Utils.h"

#include <windows.h>

#include <algorithm>
#include <set>

namespace ms {

namespace {

std::wstring TrimWs(const std::wstring& s) { return util::Trim(s); }

// 単純な識別子か
bool IsSimpleIdent(const std::wstring& s)
{
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (!(c == L'_' || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (i > 0 && c >= L'0' && c <= L'9'))) return false;
    }
    return true;
}

// プロセスを実行し、標準出力 + 標準エラーを log_path へ書き出す
bool RunCapture(const std::wstring& cmdline, const std::wstring& workdir,
                const std::wstring& log_path, DWORD timeout_ms, DWORD& out_exit)
{
    out_exit = (DWORD)-1;
    SECURITY_ATTRIBUTES sa;
    ::ZeroMemory(&sa, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hFile = ::CreateFileW(log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    STARTUPINFOW si;
    ::ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hFile;
    si.hStdError = hFile;
    si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    ::ZeroMemory(&pi, sizeof(pi));

    std::wstring cmd = cmdline;   // CreateProcessW は書き換えるのでコピー
    BOOL ok = ::CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                               CREATE_NO_WINDOW, nullptr,
                               workdir.empty() ? nullptr : workdir.c_str(), &si, &pi);
    if (!ok) {
        ::CloseHandle(hFile);
        return false;
    }
    DWORD w = ::WaitForSingleObject(pi.hProcess, timeout_ms);
    if (w == WAIT_TIMEOUT) {
        ::TerminateProcess(pi.hProcess, 124);
        ::WaitForSingleObject(pi.hProcess, 5000);
    }
    ::GetExitCodeProcess(pi.hProcess, &out_exit);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    ::CloseHandle(hFile);
    return true;
}

std::wstring Quote(const std::wstring& s) { return L"\"" + s + L"\""; }

std::wstring PathToSlash(std::wstring p)
{
    for (size_t i = 0; i < p.size(); ++i) if (p[i] == L'\\') p[i] = L'/';
    return p;
}

} // namespace

bool GTestGenerator::ParseSignature(const std::wstring& text,
                                    const std::wstring& func_name,
                                    GTestTarget& out)
{
    out.callable = false;
    out.return_type.clear();
    out.param_decls.clear();
    out.param_names.clear();

    // "func_name" の直後に '(' がある位置を探す
    size_t name_pos = std::wstring::npos;
    size_t search = 0;
    while (true) {
        size_t p = text.find(func_name, search);
        if (p == std::wstring::npos) break;
        bool left_ok = (p == 0) || !CodeScope::IsIdentifierChar(text[p - 1]);
        size_t q = p + func_name.size();
        bool right_ok = (q >= text.size()) || !CodeScope::IsIdentifierChar(text[q]);
        if (left_ok && right_ok) {
            size_t r = q;
            while (r < text.size() && (text[r] == L' ' || text[r] == L'\t')) ++r;
            if (r < text.size() && text[r] == L'(') { name_pos = p; break; }
        }
        search = p + 1;
    }
    if (name_pos == std::wstring::npos) {
        out.note = L"宣言が見つかりません";
        return false;
    }

    // 戻り値型: 直前の改行/;/}/{ までを遡る
    size_t back = name_pos;
    while (back > 0) {
        wchar_t c = text[back - 1];
        if (c == L'\n' || c == L';' || c == L'}' || c == L'{') break;
        --back;
    }
    std::wstring ret = TrimWs(text.substr(back, name_pos - back));
    // 行頭のプリプロセッサや static などの修飾子を落とす
    if (ret.find(L'#') != std::wstring::npos) {
        size_t nl = ret.find_last_of(L'#');
        if (nl != std::wstring::npos && nl + 1 <= ret.size())
            ret = TrimWs(ret.substr(nl + 1));
    }
    if (ret.empty()) {
        out.note = L"戻り値型を判定できません";
        return false;
    }
    // 直前に ')' がある場合は関数ポインタ型などなので対象外
    if (ret.find(L')') != std::wstring::npos) {
        out.note = L"複雑な宣言のため対象外";
        return false;
    }
    out.return_type = ret;

    // 引数リスト
    size_t open = text.find(L'(', name_pos + func_name.size());
    size_t close = open;
    if (open == std::wstring::npos || !CodeScope::MatchParen(text, open, close)) {
        out.note = L"引数リストを解析できません";
        return false;
    }
    std::wstring params = text.substr(open + 1, close - open - 1);
    params = util::NormalizeNewlines(params);

    std::vector<std::wstring> parts;
    std::wstring cur;
    int depth = 0;
    for (size_t i = 0; i < params.size(); ++i) {
        wchar_t c = params[i];
        if (c == L'(' || c == L'[' || c == L'<' || c == L'{') ++depth;
        else if (c == L')' || c == L']' || c == L'>' || c == L'}') --depth;
        if (c == L',' && depth == 0) { parts.push_back(cur); cur.clear(); continue; }
        cur.push_back(c);
    }
    if (!TrimWs(cur).empty()) parts.push_back(cur);

    int index = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::wstring d = TrimWs(parts[i]);
        if (d.empty() || d == L"void") continue;
        // 既定値は落とす
        size_t eq = d.find(L'=');
        if (eq != std::wstring::npos) d = TrimWs(d.substr(0, eq));
        if (d.find(L"...") != std::wstring::npos) {
            out.note = L"可変長引数は対象外";
            return false;
        }
        if (d.find(L'&') != std::wstring::npos) {
            out.note = L"参照引数は対象外";
            return false;
        }
        if (d.find(L"(*") != std::wstring::npos || d.find(L"(^") != std::wstring::npos) {
            out.note = L"関数ポインタ引数は対象外";
            return false;
        }
        // 名前を取り出す: 最後の識別子（配列なら '[' の前）
        std::wstring type_only = d;
        std::wstring name;
        size_t bracket = d.find(L'[');
        std::wstring head = (bracket == std::wstring::npos) ? d : d.substr(0, bracket);
        size_t end = head.find_last_not_of(L" \t");
        if (end != std::wstring::npos) {
            size_t s = end;
            while (s > 0 && CodeScope::IsIdentifierChar(head[s - 1])) --s;
            std::wstring cand = head.substr(s, end - s + 1);
            if (IsSimpleIdent(cand) && s > 0) {
                name = cand;
                type_only = TrimWs(head.substr(0, s)) +
                            ((bracket == std::wstring::npos) ? std::wstring() : d.substr(bracket));
            }
        }
        if (name.empty()) {
            name = L"p" + std::to_wstring(index);
            type_only = d;
        }
        if (TrimWs(type_only).empty()) {
            out.note = L"引数の型を判定できません";
            return false;
        }
        out.param_decls.push_back(TrimWs(type_only) + L" " + name + L";");
        out.param_names.push_back(name);
        ++index;
    }
    out.callable = true;
    return true;
}

std::vector<GTestTarget> GTestGenerator::CollectTargets(const std::wstring& text,
                                                        const std::wstring& file_path,
                                                        const std::vector<MatchResult>& matches)
{
    std::vector<GTestTarget> out;
    std::vector<std::wstring> seen;
    for (size_t i = 0; i < matches.size(); ++i) {
        std::wstring fn = CodeScope::EnclosingFunctionName(text, matches[i].start_offset);
        if (fn.empty()) continue;
        if (std::find(seen.begin(), seen.end(), fn) != seen.end()) continue;
        seen.push_back(fn);

        GTestTarget t;
        t.file_path = file_path;
        t.func_name = fn;
        t.is_c = (util::ToUpper(util::GetExtensionOf(file_path)) == L".C");
        ParseSignature(text, fn, t);
        out.push_back(t);
    }
    return out;
}

std::wstring GTestGenerator::DefaultOutDir(const std::wstring& root_dir)
{
    return util::JoinPath(root_dir, L"tests\\gtest");
}

bool GTestGenerator::WriteProject(const std::wstring& root_dir,
                                  const std::vector<GTestTarget>& targets,
                                  const GTestOptions& opt,
                                  std::vector<std::wstring>& out_files,
                                  std::wstring& out_error)
{
    out_error.clear();
    out_files.clear();
    if (targets.empty()) {
        out_error = L"テスト対象の関数がありません。";
        return false;
    }
    std::wstring dir = opt.out_dir.empty() ? DefaultOutDir(root_dir) : opt.out_dir;
    if (!util::EnsureDirectory(dir)) {
        out_error = L"出力先を作成できません: " + dir;
        return false;
    }

    // ---- generated_tests.cpp ----
    std::vector<std::wstring> srcs;
    std::wstring code;
    code += L"// =============================================================================\n";
    code += L"// generated_tests.cpp - generated by CopyMigration Switcher\n";
    code += L"// GoogleTest for the functions that contain migration switches.\n";
    code += L"// This file is ASCII only.\n";
    code += L"//\n";
    code += L"// Build:\n";
    code += L"//   cmake -S . -B build\n";
    code += L"//   cmake --build build --config Release\n";
    code += L"// Run:\n";
    code += L"//   build/Release/migration_gtest.exe\n";
    code += L"//   MS_LOAD_ITERATIONS=0 -> skip the load tests\n";
    code += L"// =============================================================================\n";
    code += L"#include <gtest/gtest.h>\n\n";
    code += L"#include <chrono>\n#include <cstdlib>\n#include <cstdint>\n#include <iostream>\n#include <string>\n\n";

    for (size_t i = 0; i < targets.size(); ++i) {
        if (targets[i].is_c) {
            code += L"extern \"C\" {\n";
            code += L"    " + targets[i].return_type + L" " + targets[i].func_name + L"(";
            for (size_t k = 0; k < targets[i].param_decls.size(); ++k) {
                if (k) code += L", ";
                code += targets[i].param_decls[k];
            }
            if (targets[i].param_decls.empty()) code += L"void";
            code += L");\n};\n\n";
        }
    }

    code += L"static int LoadIterations()\n{\n"
            L"    const char* e = std::getenv(\"MS_LOAD_ITERATIONS\");\n"
            L"    if (!e || !*e) return " + std::to_wstring(opt.load_iterations) + L";\n"
            L"    int v = std::atoi(e);\n"
            L"    return (v < 0) ? 0 : v;\n}\n\n";

    for (size_t i = 0; i < targets.size(); ++i) {
        const GTestTarget& t = targets[i];
        std::wstring base = util::GetBaseNameNoExt(t.file_path);
        std::wstring safe;
        for (size_t k = 0; k < base.size(); ++k)
            safe += CodeScope::IsIdentifierChar(base[k]) ? base[k] : L'_';
        std::wstring suite = safe + L"_" + t.func_name;
        if (!CodeScope::IsIdentifierChar(suite[0]) || (suite[0] >= L'0' && suite[0] <= L'9'))
            suite = L"T_" + suite;

        code += L"// " + t.file_path + L"\n";
        if (!t.note.empty()) code += L"// NOTE: " + t.note + L"\n";

        if (!t.callable) {
            code += L"TEST(" + suite + L", DISABLED_Smoke)\n{\n";
            code += L"    // TODO: " + (t.note.empty() ? std::wstring(L"fill in the arguments") : t.note) + L"\n";
            code += L"    GTEST_SKIP() << \"signature not supported by the generator\";\n}\n\n";
            continue;
        }

        code += L"TEST(" + suite + L", Smoke)\n{\n";
        code += L"    // TODO: replace the default arguments with real values.\n";
        for (size_t k = 0; k < t.param_decls.size(); ++k)
            code += L"    " + t.param_decls[k] + L"\n";
        code += L"    ";
        if (t.return_type != L"void") code += L"(void)";
        code += t.func_name + L"(";
        for (size_t k = 0; k < t.param_names.size(); ++k) {
            if (k) code += L", ";
            code += t.param_names[k];
        }
        code += L");\n}\n\n";

        if (opt.generate_load) {
            code += L"TEST(" + suite + L", Load)\n{\n";
            code += L"    const int iters = LoadIterations();\n";
            code += L"    if (iters <= 0) GTEST_SKIP() << \"MS_LOAD_ITERATIONS=0\";\n";
            for (size_t k = 0; k < t.param_decls.size(); ++k)
                code += L"    " + t.param_decls[k] + L"\n";
            code += L"    const auto t0 = std::chrono::steady_clock::now();\n";
            code += L"    for (int i = 0; i < iters; ++i) {\n        ";
            if (t.return_type != L"void") code += L"(void)";
            code += t.func_name + L"(";
            for (size_t k = 0; k < t.param_names.size(); ++k) {
                if (k) code += L", ";
                code += t.param_names[k];
            }
            code += L");\n    }\n";
            code += L"    const auto t1 = std::chrono::steady_clock::now();\n";
            code += L"    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();\n";
            code += L"    std::cout << \"[LOAD] " + t.func_name + L": \" << iters << \" iters, \"\n";
            code += L"              << ms << \" ms total, \" << (ms * 1000.0 / (double)iters)\n";
            code += L"              << \" us/call\" << std::endl;\n";
            code += L"    EXPECT_GT(iters, 0);\n}\n\n";
        }
    }

    std::wstring test_path = util::JoinPath(dir, L"generated_tests.cpp");
    std::wstring werr;
    if (!FileManager::WriteTextAtomic(test_path, code, TextEncoding::UTF8, false,
                                      NewlineKind::CRLF, werr)) {
        out_error = L"テストを書き出せません: " + werr;
        return false;
    }
    out_files.push_back(test_path);

    // ---- CMakeLists.txt ----
    std::set<std::wstring> uniq;
    for (size_t i = 0; i < targets.size(); ++i) uniq.insert(targets[i].file_path);

    std::wstring cm;
    cm += L"# generated by CopyMigration Switcher\n";
    cm += L"cmake_minimum_required(VERSION 3.16)\n";
    cm += L"project(" + opt.project_name + L" LANGUAGES C CXX)\n\n";
    cm += L"set(CMAKE_CXX_STANDARD 17)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n";
    cm += L"set(MS_SOURCES\n";
    for (std::set<std::wstring>::const_iterator it = uniq.begin(); it != uniq.end(); ++it)
        cm += L"    \"" + PathToSlash(*it) + L"\"\n";
    cm += L")\n\n";
    cm += L"find_package(GTest CONFIG QUIET)\n";
    cm += L"if(NOT GTest_FOUND)\n";
    cm += L"    include(FetchContent)\n";
    cm += L"    FetchContent_Declare(googletest\n";
    cm += L"        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)\n";
    cm += L"    set(gtest_force_shared_crt ON CACHE BOOL \"\" FORCE)\n";
    cm += L"    set(INSTALL_GTEST OFF CACHE BOOL \"\" FORCE)\n";
    cm += L"    FetchContent_MakeAvailable(googletest)\n";
    cm += L"endif()\n\n";
    cm += L"add_executable(migration_gtest generated_tests.cpp ${MS_SOURCES})\n";
    cm += L"target_include_directories(migration_gtest PRIVATE \"" +
          PathToSlash(root_dir) + L"\")\n";
    cm += L"target_link_libraries(migration_gtest PRIVATE GTest::gtest GTest::gtest_main)\n\n";
    cm += L"enable_testing()\n";
    cm += L"add_test(NAME migration_gtest COMMAND migration_gtest)\n";
    std::wstring cm_path = util::JoinPath(dir, L"CMakeLists.txt");
    if (!FileManager::WriteTextAtomic(cm_path, cm, TextEncoding::UTF8, false,
                                      NewlineKind::CRLF, werr)) {
        out_error = L"CMakeLists.txt を書き出せません: " + werr;
        return false;
    }
    out_files.push_back(cm_path);
        // スイッチを外部から切り替えられるように CMake へ追記する
    if (!opt.switch_name.empty()) {
        std::wstring add;
        add += L"\n# ---- migration switch (external toggle) ----\n";
        add += L"set(MS_SWITCH_ON OFF CACHE BOOL \"Build with the migration switch enabled\")\n";
        add += L"if(MS_SWITCH_ON)\n";
        add += L"    add_compile_definitions(" + opt.switch_name + L")\n";
        add += L"endif()\n";
        std::wstring cur;
        FileInfo fi2;
        if (FileManager::ReadText(cm_path, L"auto", cur, fi2)) {
            std::wstring e2;
            FileManager::WriteTextAtomic(cm_path, cur + add, TextEncoding::UTF8,
                                         false, NewlineKind::CRLF, e2);
        }
    }
return true;
}

bool GTestGenerator::BuildAndRun(const std::wstring& root_dir,
                                 const GTestOptions& opt,
                                 GTestRunResult& out,
                                 std::wstring& out_error)
{
    out_error.clear();
    out = GTestRunResult();

    std::wstring dir = opt.out_dir.empty() ? DefaultOutDir(root_dir) : opt.out_dir;
    std::wstring build = util::JoinPath(root_dir, L"build\\gtest");
    std::wstring logdir = util::JoinPath(root_dir, L"reports\\gtest");
    if (!util::EnsureDirectory(build) || !util::EnsureDirectory(logdir)) {
        out_error = L"作業フォルダを作成できません。";
        return false;
    }

    wchar_t tmp[MAX_PATH];
    ::GetTempPathW(MAX_PATH, tmp);
    std::wstring stamp = util::TimestampFileName();
    std::wstring configure_log = util::JoinPath(logdir, L"configure_" + stamp + L".log");
    std::wstring build_log = util::JoinPath(logdir, L"build_" + stamp + L".log");
    std::wstring run_log = util::JoinPath(logdir, L"run_" + stamp + L".log");
    std::wstring json = util::JoinPath(logdir, L"result_" + stamp + L".json");

    DWORD rc = 0;
    // 1) configure
    std::wstring cmd = L"cmake -S " + Quote(dir) + L" -B " + Quote(build) +
                       L" -DCMAKE_BUILD_TYPE=Release";
    if (!RunCapture(cmd, root_dir, configure_log, 600000, rc) || rc != 0) {
        out.build_log = configure_log;
        out_error = L"CMake の構成に失敗しました（exit " + std::to_wstring((int)rc) +
                    L"）。ログ: " + configure_log;
        return false;
    }
    // 2) build
    cmd = L"cmake --build " + Quote(build) + L" --config Release";
    if (!RunCapture(cmd, root_dir, build_log, 1800000, rc) || rc != 0) {
        out.build_log = build_log;
        out_error = L"テストのビルドに失敗しました（exit " + std::to_wstring((int)rc) +
                    L"）。ログ: " + build_log;
        return false;
    }
    out.built = true;

    // 3) exe を探す
    std::wstring candidates[3] = {
        util::JoinPath(build, L"Release\\migration_gtest.exe"),
        util::JoinPath(build, L"migration_gtest.exe"),
        util::JoinPath(build, L"Debug\\migration_gtest.exe")
    };
    std::wstring exe;
    for (int i = 0; i < 3; ++i)
        if (util::PathExists(candidates[i])) { exe = candidates[i]; break; }
    if (exe.empty()) {
        out.build_log = build_log;
        out_error = L"テスト実行ファイルが見つかりません: " + build;
        return false;
    }
    out.exe_path = exe;

    // 4) run
    cmd = Quote(exe) + L" --gtest_output=json:" + Quote(json);
    if (!RunCapture(cmd, root_dir, run_log, 1800000, rc)) {
        out_error = L"テストを実行できませんでした。";
        return false;
    }
    out.ran = true;
    out.run_log = run_log;
    out.json_path = json;

    std::wstring js;
    FileInfo fi;
    if (FileManager::ReadText(json, L"auto", js, fi)) {
        size_t p = js.find(L"\"tests\"");
        if (p != std::wstring::npos) {
            size_t c = js.find(L':', p);
            if (c != std::wstring::npos) out.tests = _wtoi(js.c_str() + c + 1);
        }
        p = js.find(L"\"failures\"");
        if (p != std::wstring::npos) {
            size_t c = js.find(L':', p);
            if (c != std::wstring::npos) out.failures = _wtoi(js.c_str() + c + 1);
        }
        p = js.find(L"\"disabled\"");
        if (p != std::wstring::npos) {
            size_t c = js.find(L':', p);
            if (c != std::wstring::npos) out.disabled = _wtoi(js.c_str() + c + 1);
        }
        p = js.find(L"\"time\"");
        if (p != std::wstring::npos) {
            size_t c = js.find(L':', p);
            if (c != std::wstring::npos) out.seconds = _wtof(js.c_str() + c + 1);
        }
    }

    out.summary = L"テスト " + std::to_wstring(out.tests) +
                  L" 件 / 失敗 " + std::to_wstring(out.failures) +
                  L" / 無効 " + std::to_wstring(out.disabled) +
                  L"（" + std::to_wstring(out.seconds) + L" 秒）";
    return true;
}

} // namespace ms

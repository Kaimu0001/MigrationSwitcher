// =============================================================================
// CodeLogger.cpp
// =============================================================================
#include "CodeLogger.h"
#include "CodeScope.h"
#include "Utils.h"

#include <algorithm>
#include <set>

namespace ms {

namespace {

// 文字列・文字リテラル・コメントを考慮してキーワード位置を列挙する
struct Scanner {
    const std::wstring& t;
    explicit Scanner(const std::wstring& s) : t(s) {}

    // pos 以降で最初に現れる識別子 word の位置（コメント・文字列は除外）
    size_t FindWord(size_t from, size_t to, const std::wstring& word) const
    {
        size_t i = from;
        while (i < to && i < t.size()) {
            wchar_t c = t[i];
            if (c == L'/' && i + 1 < t.size() && t[i + 1] == L'/') {
                while (i < t.size() && t[i] != L'\n') ++i;
                continue;
            }
            if (c == L'/' && i + 1 < t.size() && t[i + 1] == L'*') {
                i += 2;
                while (i + 1 < t.size() && !(t[i] == L'*' && t[i + 1] == L'/')) ++i;
                i = (i + 1 < t.size()) ? i + 2 : t.size();
                continue;
            }
            if (c == L'"' || c == L'\'') {
                wchar_t q = c;
                ++i;
                while (i < t.size()) {
                    if (t[i] == L'\\') { i += 2; continue; }
                    if (t[i] == q) { ++i; break; }
                    if (t[i] == L'\n' && q == L'"') break;
                    ++i;
                }
                continue;
            }
            if (CodeScope::IsIdentifierChar(c)) {
                size_t s = i;
                while (i < t.size() && CodeScope::IsIdentifierChar(t[i])) ++i;
                if (i - s == word.size() && t.compare(s, i - s, word) == 0)
                    return s;
                continue;
            }
            ++i;
        }
        return std::wstring::npos;
    }
};

} // namespace

bool CodeLogger::HasInclude(const std::wstring& text, const std::wstring& header)
{
    std::wstring needle = L"#include \"" + header + L"\"";
    std::wstring needle2 = L"#include <" + header + L">";
    return text.find(needle) != std::wstring::npos ||
           text.find(needle2) != std::wstring::npos;
}

std::wstring CodeLogger::AddInclude(const std::wstring& text, const std::wstring& header,
                                    bool& out_added)
{
    out_added = false;
    if (HasInclude(text, header)) return text;

    std::wstring line = L"#include \"" + header + L"\"";
    std::vector<std::wstring> lines = util::SplitLines(text);
    int last_include = -1;
    bool in_comment = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring tr = util::Trim(lines[i]);
        if (in_comment) {
            if (tr.find(L"*/") != std::wstring::npos) in_comment = false;
            continue;
        }
        if (tr.find(L"/*") != std::wstring::npos && tr.find(L"*/") == std::wstring::npos) {
            in_comment = true;
            continue;
        }
        if (tr.size() > 8 && tr.compare(0, 8, L"#include") == 0) last_include = (int)i;
    }
    if (last_include < 0) {
        lines.insert(lines.begin(), line);
    } else {
        lines.insert(lines.begin() + last_include + 1, line);
    }
    out_added = true;
    return util::JoinLines(lines, L"\n");
}

bool CodeLogger::Apply(std::wstring& text,
                       const std::vector<MatchResult>& selected,
                       const LogInsertOptions& opt,
                       LogInsertResult& out,
                       std::wstring& out_error)
{
    out = LogInsertResult();
    out_error.clear();
    if (selected.empty()) {
        out_error = L"置換対象が選択されていません。";
        return false;
    }
    if (!opt.insert_input && !opt.insert_output && !opt.insert_temp) return true;

    // ---- 対象関数を集める（出現順を維持）----
    std::vector<std::wstring> funcs;
    for (size_t i = 0; i < selected.size(); ++i) {
        std::wstring fn = CodeScope::EnclosingFunctionName(text, selected[i].start_offset);
        if (fn.empty()) {
            out.warnings.push_back(L"関数を判定できない一致があります（" +
                                   std::to_wstring(selected[i].start_line) + L" 行目）。ログは挿入していません。");
            continue;
        }
        if (std::find(funcs.begin(), funcs.end(), fn) == funcs.end()) funcs.push_back(fn);
    }

    // 挿入位置を後ろから適用する
    struct Ins { size_t pos; std::wstring text; int kind; };
    std::vector<Ins> ins;

    for (size_t f = 0; f < funcs.size(); ++f) {
        int s = 0, e = 0;
        std::wstring err;
        if (!CodeScope::FindFunctionBody(text, funcs[f], s, e, err)) {
            out.warnings.push_back(L"関数本体を特定できません: " + funcs[f]);
            continue;
        }
        size_t line_start = util::LineStartOffset(text, s);
        size_t brace = text.find(L'{', line_start);
        if (brace == std::wstring::npos) {
            out.warnings.push_back(L"関数本体の '{' を特定できません: " + funcs[f]);
            continue;
        }
        size_t close = brace;
        if (!CodeScope::MatchBrace(text, brace, close)) close = text.size();
        close = (close > brace) ? close : brace;

        Scanner sc(text);
        bool already = sc.FindWord(brace, close, L"INPUT_LOG") != std::wstring::npos ||
                       sc.FindWord(brace, close, L"OUTPUT_LOG") != std::wstring::npos;
        if (already) {
            out.warnings.push_back(L"既にログ挿入済みのためスキップしました: " + funcs[f]);
            continue;
        }
        ++out.functions;

        // --- INPUT_LOG: '{' の行の直後 ---
        if (opt.insert_input) {
            size_t nl = text.find(L'\n', brace);
            size_t at = (nl == std::wstring::npos) ? text.size() : nl + 1;
            std::wstring indent = util::LeadingIndentText(text.substr(line_start, std::min(text.size() - line_start, brace - line_start)));
            Ins in;
            in.pos = at;
            in.text = indent + L"    INPUT_LOG(\"" + funcs[f] + L": enter\");\n";
            in.kind = 1;
            ins.push_back(in);
        }

        // --- OUTPUT_LOG: 各 return の直前 ---
        if (opt.insert_output) {
            size_t i = brace;
            while (true) {
                size_t r = sc.FindWord(i, close, L"return");
                if (r == std::wstring::npos) break;
                size_t ls = text.rfind(L'\n', r);
                ls = (ls == std::wstring::npos) ? 0 : ls + 1;
                std::wstring indent = util::LeadingIndentText(text.substr(ls, r - ls));

                // 戻り値の式（単純な数値リテラル / 識別子のみ）
                size_t j = r + 6;
                while (j < text.size() && (text[j] == L' ' || text[j] == L'\t')) ++j;
                std::wstring expr;
                if (j < text.size() && text[j] != L';') {
                    size_t k = j;
                    bool numeric = true;
                    while (k < text.size() && text[k] != L';' && text[k] != L'\n') {
                        wchar_t c = text[k];
                        if (!(c == L'-' || c == L'+' || c == L'.' || (c >= L'0' && c <= L'9') ||
                              CodeScope::IsIdentifierChar(c))) numeric = false;
                        ++k;
                    }
                    std::wstring raw = util::Trim(text.substr(j, k - j));
                    bool simple = !raw.empty() && raw.size() <= 60 &&
                                  !CodeScope::IsIdentifierChar(L' ');
                    // 単純な識別子か数値リテラルのみ許可（式の二重評価を避ける）
                    bool ok_ident = true;
                    for (size_t q = 0; q < raw.size(); ++q) {
                        wchar_t c = raw[q];
                        if (!(c == L'_' || c == L'-' || c == L'+' || c == L'.' ||
                              (c >= L'0' && c <= L'9') ||
                              (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'))) {
                            ok_ident = false; break;
                        }
                    }
                    if (simple && ok_ident) expr = raw;
                    (void)numeric;
                }

                std::wstring call;
                if (opt.log_return_value && !expr.empty()) {
                    call = indent + L"OUTPUT_LOG(\"return %ld\", (long)(" + expr + L"));\n";
                } else if (!expr.empty() && !opt.log_return_value) {
                    call = indent + L"OUTPUT_LOG(\"return\");\n";
                } else {
                    call = indent + L"OUTPUT_LOG(\"return\");\n";
                }
                Ins o;
                o.pos = ls;
                o.text = call;
                o.kind = 2;
                ins.push_back(o);

                i = r + 6;
            }
        }
        if (!opt.insert_output) {
            // INPUT のみでも関数数を数える
        }
    }

    // --- TEMP_LOG: 追加された #ifdef <switch> の直後 ---
    if (opt.insert_temp && !opt.switch_name.empty()) {
        std::wstring needle = L"#ifdef " + opt.switch_name;
        size_t p = 0;
        while (true) {
            size_t q = text.find(needle, p);
            if (q == std::wstring::npos) break;
            size_t nl = text.find(L'\n', q);
            if (nl == std::wstring::npos) break;
            size_t at = nl + 1;
            std::wstring indent = util::LeadingIndentText(
                text.substr(text.rfind(L'\n', q) == std::wstring::npos ? 0 : text.rfind(L'\n', q) + 1,
                            q - (text.rfind(L'\n', q) == std::wstring::npos ? 0 : text.rfind(L'\n', q) + 1)));
            std::wstring next = text.substr(at, std::min<size_t>(80, text.size() - at));
            if (next.find(L"TEMP_LOG(") == std::wstring::npos) {
                Ins t;
                t.pos = at;
                t.text = indent + L"    TEMP_LOG(\"" + opt.switch_name + L": new branch\");\n";
                t.kind = 3;
                ins.push_back(t);
            }
            p = nl + 1;
        }
    }

    // 後ろから挿入する（同じ位置は kind の大きい順で安定させる）
    std::stable_sort(ins.begin(), ins.end(), [](const Ins& a, const Ins& b) {
        if (a.pos != b.pos) return a.pos > b.pos;
        return a.kind > b.kind;
    });
    for (size_t i = 0; i < ins.size(); ++i) {
        if (ins[i].pos > text.size()) continue;
        text.insert(ins[i].pos, ins[i].text);
        if (ins[i].kind == 1) ++out.input_logs;
        else if (ins[i].kind == 2) ++out.output_logs;
        else ++out.temp_logs;
    }

    if (out.input_logs + out.output_logs + out.temp_logs > 0) {
        bool added = false;
        text = AddInclude(text, opt.header_name, added);
        out.include_added = added;
    }
    return true;
}

} // namespace ms

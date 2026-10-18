// =============================================================================
// DiffEngine.h
// 設計書 12章「差分生成設計」の行差分アルゴリズムと表示部品
// DiffReportGenerator から切り出し、単一／統合レポートで共用する
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

// 12.3 HTML レポート CSS 方針に対応する行種別
enum DiffLineType {
    DIFF_NORMAL = 0,
    DIFF_ADDED,
    DIFF_DELETED,
    DIFF_CHANGED
};

struct DiffLine {
    DiffLineType type;
    int          line_no;
    std::wstring text;
};

class DiffEngine {
public:
    // 12.2 行単位の差分（LCS）。規模が大きい場合はブロック差分へフォールバックする
    static void ComputeLineDiff(const std::vector<std::wstring>& before,
                                const std::vector<std::wstring>& after,
                                std::vector<DiffLine>& out);

    // 12.3 色分け（added / deleted / changed / normal）
    static const wchar_t* ClassName(DiffLineType t);

    // 差分表（<table class="diff"> の内部）を HTML 断片として生成する
    static std::wstring RenderRows(const std::vector<DiffLine>& diff);

    // 対象箇所の前後 context 行を切り出す（行番号は 1 起点で返す）
    static void MakeExcerpt(const std::vector<std::wstring>& before,
                            const std::vector<std::wstring>& after,
                            int first_line, int last_line, int context,
                            std::vector<std::wstring>& out_before,
                            std::vector<std::wstring>& out_after,
                            int& out_before_from, int& out_after_from);

    // 行番号付き <pre> の中身
    static std::wstring PreWithLineNumbers(const std::vector<std::wstring>& lines,
                                           int first_line);
};

} // namespace ms

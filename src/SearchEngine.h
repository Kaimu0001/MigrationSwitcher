// =============================================================================
// SearchEngine.h
// 設計書 6.5「検索機能」/ 6.6「複数ヒット処理」/ 15章 既存スイッチ検出 / 16章 コメント内ヒット
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

class SearchEngine {
public:
    // 6.5.3 完全一致検索。text は改行 LF 正規化済みであることを前提とする。
    static std::vector<MatchResult> Search(const std::wstring& text,
                                           const std::wstring& pattern,
                                           const std::wstring& file_path);

    // 15章 既存コンパイルスイッチ検出（対象行の前後 scan_lines 行）
    static bool DetectNearbyPreprocessor(const std::wstring& text,
                                         int start_line, int end_line,
                                         int scan_lines,
                                         std::wstring& out_detail);

    // 16章 コメント内ヒット判定（簡易判定）
    static bool IsInsideComment(const std::wstring& text, size_t offset);

    // 指定した一致箇所をまとめて置換する（後ろから置換して位置ずれを防ぐ）
    static std::wstring ApplyReplacements(const std::wstring& text,
                                          std::vector<MatchResult>& matches,
                                          const std::wstring& replacement_for_all);

    // 一致箇所ごとに異なる置換文字列を適用する（blocks[i] は matches[i] に対応）
    static std::wstring ApplyReplacementsEach(const std::wstring& text,
                                              const std::vector<MatchResult>& matches,
                                              const std::vector<std::wstring>& blocks);

    // 6.8 インデント自動調整用: 一致箇所の行頭インデント（空白・タブのみ）を取得する
    static std::wstring IndentHintAt(const std::wstring& text, const MatchResult& m);

    // 連番を振り直す
    static void Renumber(std::vector<MatchResult>& matches);
};

} // namespace ms

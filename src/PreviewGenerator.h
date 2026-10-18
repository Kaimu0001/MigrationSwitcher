// =============================================================================
// PreviewGenerator.h / BackupManager.h / UndoManager.h 相当の宣言は各ファイルに分割
// -----------------------------------------------------------------------------
// PreviewGenerator.h
// 設計書 5.7「プレビュータブ」/ 7.2.6 PreviewGenerator
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

class PreviewGenerator {
public:
    // 対象箇所の前後 context_lines 行を切り出して行番号付きで返す
    static std::wstring MakeExcerpt(const std::wstring& text,
                                    int start_line, int end_line,
                                    int context_lines,
                                    int& out_start_line);

    // 5.7.2 表示モード別の整形
    static std::wstring BuildSimple(const std::wstring& after_excerpt);
    static std::wstring BuildTopBottom(const std::wstring& before_excerpt,
                                       const std::wstring& after_excerpt);
    static std::wstring BuildSideBySide(const std::wstring& before_excerpt,
                                        const std::wstring& after_excerpt);

    // 選択された全一致箇所をまとめて差し替えた仮想ファイル内容を生成
    static std::wstring BuildAfterText(const std::wstring& text,
                                       std::vector<MatchResult>& matches,
                                       const std::wstring& block_for_all);

    // 一致箇所ごとに異なるブロックで差し替えた仮想ファイル内容を生成（一括処理用）
    // 行中一致（例: "    return old_function(x);"）は行頭ホイストを行い、
    // #ifdef/#else/#endif を行頭へ移して前後のコードを各分岐へ複製する。
    static std::wstring BuildAfterTextEach(const std::wstring& text,
                                           const std::vector<MatchResult>& matches,
                                           const std::vector<std::wstring>& blocks);

    // 行中一致のホイストが必要か（行全体の一致なら false）
    static bool NeedsHoist(const std::wstring& text, const MatchResult& m,
                           size_t& out_line_start, size_t& out_line_end);

    // ブロックをホイストした文字列を返す（構造が取れない場合は元のブロックを返す）
    static std::wstring HoistBlock(const std::wstring& text, const MatchResult& m,
                                   const std::wstring& block,
                                   size_t line_start, size_t line_end);

    // 変更前／変更後の抜粋（選択箇所全体をカバーする）
    static bool BuildExcerpts(const std::wstring& before_text,
                              const std::wstring& after_text,
                              const std::vector<MatchResult>& matches,
                              int context_lines,
                              std::wstring& out_before,
                              std::wstring& out_after);
};

} // namespace ms

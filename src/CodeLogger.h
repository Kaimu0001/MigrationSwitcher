// =============================================================================
// CodeLogger.h
// 追加機能: 置換対象コードへ INPUT_LOG / OUTPUT_LOG / TEMP_LOG の呼び出しを挿入する
//   - 置換対象を含む関数の入口に INPUT_LOG
//   - その関数内の各 return の直前に OUTPUT_LOG
//   - TEMP_LOG を有効にすると、追加した #ifdef 直後に分岐記録を入れる
// =============================================================================
#pragma once

#include <string>
#include <vector>

#include "Models.h"

namespace ms {

struct LogInsertOptions {
    std::wstring header_name;      // 既定: ms_migration_log.h
    std::wstring switch_name;      // TEMP_LOG の分岐記録に使う
    bool         insert_input  = true;
    bool         insert_output = true;
    bool         insert_temp   = false;
    bool         log_return_value = false;  // true: (long) キャストで戻り値も記録（整数型のみ）

    LogInsertOptions() : header_name(L"ms_migration_log.h") {}
};

struct LogInsertResult {
    int  functions     = 0;
    int  input_logs    = 0;
    int  output_logs   = 0;
    int  temp_logs     = 0;
    bool include_added = false;
    std::vector<std::wstring> warnings;
};

class CodeLogger {
public:
    // text を書き換える（LF 正規化済みの内部表現）。
    static bool Apply(std::wstring& text,
                      const std::vector<MatchResult>& selected,
                      const LogInsertOptions& opt,
                      LogInsertResult& out,
                      std::wstring& out_error);

    // #include "..." が既にあるか
    static bool HasInclude(const std::wstring& text, const std::wstring& header);

    // #include 行を追加したテキストを返す（追加できなければ元のまま）
    static std::wstring AddInclude(const std::wstring& text, const std::wstring& header,
                                   bool& out_added);
};

} // namespace ms

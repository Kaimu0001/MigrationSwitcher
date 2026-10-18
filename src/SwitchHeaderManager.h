// =============================================================================
// SwitchHeaderManager.h
// スイッチ定義ヘッダ（例: migration_switch.h）の一元管理
//
// これまで各ソース内に散在していたコンパイルスイッチの「定義」を 1 つの
// ヘッダファイルへ集約し、対象ソースは #include して #ifdef で参照する。
// スイッチの追加・有効化・無効化・一覧をこのモジュールが担う。
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

// スイッチ 1 件分の情報
struct SwitchEntry {
    std::wstring name;          // スイッチ名（例: MIGRATION_001）
    std::wstring comment;       // 用途メモ（人が読む説明）
    std::wstring summary;       // 旧→新の要約（自動生成）
    std::vector<std::wstring> files;   // このスイッチを使用しているソースファイル名
    std::wstring updated_at;    // 最終更新日時
    bool         enabled = false;      // #define が有効か
};

// ヘッダファイルの解析結果
struct SwitchHeaderInfo {
    std::wstring path;
    bool         exists = false;
    bool         has_managed_section = false;
    std::vector<SwitchEntry> entries;
    std::wstring before_section;   // 管理領域より前の内容
    std::wstring after_section;    // 管理領域より後の内容
    std::wstring guard;            // インクルードガード名
};

class SwitchHeaderManager {
public:
    // ---- パス ----
    static std::wstring DefaultHeaderName();                              // migration_switch.h
    static std::wstring GuardFromName(const std::wstring& header_name);
    static bool ValidateHeaderName(const std::wstring& header_name, std::wstring& out_error);

    // ヘッダの絶対パスを決める（header_dir が空なら base_dir と同一フォルダ）
    static std::wstring ResolveHeaderPath(const std::wstring& base_dir,
                                          const std::wstring& header_dir,
                                          const std::wstring& header_name);

    // source_path から header_path を参照する #include 用の文字列を返す
    // （同一フォルダなら "name.h"、別フォルダなら相対パス、不可なら絶対パス）
    static std::wstring MakeIncludePath(const std::wstring& source_path,
                                        const std::wstring& header_path);

    // ---- 読み込み・書き込み ----
    static bool Load(const std::wstring& header_path, SwitchHeaderInfo& out);
    static bool Write(const std::wstring& header_path,
                      const SwitchHeaderInfo& info,
                      std::wstring& out_error);

    // スイッチを登録する（既存なら files / comment / summary を更新して重複を作らない）
    // out_added: 新規追加された場合 true
    static bool RegisterSwitch(const std::wstring& header_path,
                               const std::wstring& switch_name,
                               const std::wstring& memo,
                               const std::wstring& old_code,
                               const std::wstring& new_code,
                               const std::vector<std::wstring>& source_files,
                               SwitchHeaderInfo& out_info,
                               bool& out_added,
                               std::wstring& out_error);

    // 有効／無効を切り替える
    static bool SetEnabled(const std::wstring& header_path,
                           const std::wstring& switch_name,
                           bool enabled,
                           SwitchHeaderInfo& out_info,
                           std::wstring& out_error);

    // スイッチを管理領域から削除する
    static bool RemoveSwitch(const std::wstring& header_path,
                             const std::wstring& switch_name,
                             SwitchHeaderInfo& out_info,
                             std::wstring& out_error);

    // ---- ソースへの #include 追加 ----
    // text は LF 正規化済み。追加した場合は out_added = true / out_line は 1 起点行番号
    static bool EnsureInclude(std::wstring& text,
                              const std::wstring& include_path,
                              bool& out_added,
                              int& out_line,
                              std::wstring& out_error);

    static bool HasInclude(const std::wstring& text, const std::wstring& include_path);

    // 要約文字列を作る（例: "old_function(); -> new_function();"）
    static std::wstring MakeSummary(const std::wstring& old_code,
                                    const std::wstring& new_code);

private:
    static std::wstring BuildManagedSection(const std::vector<SwitchEntry>& entries);
    static std::wstring BuildFullContent(const SwitchHeaderInfo& info);
    static void ParseManagedSection(const std::wstring& section,
                                    std::vector<SwitchEntry>& out);
    static std::wstring FirstMeaningfulLine(const std::wstring& code, size_t max_len);
};

} // namespace ms

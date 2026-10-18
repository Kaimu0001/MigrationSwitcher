// =============================================================================
// Models.h
// Migration Switcher - データモデル定義
// 設計書 8章「データ設計」/ 6.5.4「検索結果」に対応
// =============================================================================
#pragma once

#include <string>
#include <vector>

namespace ms {

// -----------------------------------------------------------------------------
// 置換方式（設計書 6.7 変換方式A～D + 18章 テンプレート）
// -----------------------------------------------------------------------------
enum class SwitchStyle {
    IfdefNewFirst = 0,   // 方式A: #ifdef 新コード優先方式
    IfndefOldFirst,      // 方式B: #ifndef 旧コード優先方式
    IfdefWithComments,   // 方式C: コメント付き方式
    BlockCommentStyle,   // 方式D: 開始終了コメント方式
    CustomTemplate       // 18章: ユーザーテンプレート
};

// -----------------------------------------------------------------------------
// 一致種別（設計書 5.6.1 検索結果カラム）
// -----------------------------------------------------------------------------
enum class MatchType {
    Exact = 0,      // 完全一致
    Normalized,     // 正規化一致（後続版）
    Similar         // 類似一致（後続版）
};

// -----------------------------------------------------------------------------
// 一致箇所の状態（設計書 5.6.1 / 8.3）
// -----------------------------------------------------------------------------
enum class MatchStatus {
    Target = 0,     // 対象
    Excluded,       // 除外
    Warning,        // 警告
    Error           // エラー
};

// -----------------------------------------------------------------------------
// 実行モード（設計書 6.11 ドライラン / 6.12 レポート）
// -----------------------------------------------------------------------------
enum class ExecutionMode {
    DryRun = 0,     // ドライラン
    Real            // 本実行
};

// -----------------------------------------------------------------------------
// ヒット時の動作（設計書 5.3.1 [ヒット時の動作]）
// -----------------------------------------------------------------------------
enum class HitBehavior {
    AutoSelectSingle = 0,  // 1件の場合のみ自動選択
    FirstOnly,             // 最初の1件を対象にする
    AllMatches,            // すべての一致を対象にする
    Manual                 // ユーザーが一覧から選択する
};

// -----------------------------------------------------------------------------
// プレビュー表示モード（設計書 5.7.2）
// -----------------------------------------------------------------------------
enum class PreviewMode {
    Simple = 0,     // 単純表示
    TopBottom,      // 前後比較表示
    SideBySide,     // 左右比較表示
    Html            // HTML表示
};

// -----------------------------------------------------------------------------
// アプリケーション状態（設計書 13.1）
// -----------------------------------------------------------------------------
enum class AppState {
    INIT = 0,
    READY,
    FILE_SELECTED,
    SEARCHED,
    PREVIEWED,
    EXECUTING,
    COMPLETED,
    Error,          // Windows の ERROR マクロ（wingdi.h）と衝突するため Error とする
    UNDO_AVAILABLE
};

// -----------------------------------------------------------------------------
// ログレベル（設計書 6.14.3）
// -----------------------------------------------------------------------------
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    Error,          // Windows の ERROR マクロと衝突するため Error とする
    FATAL
};

// -----------------------------------------------------------------------------
// テキスト文字コード（設計書 3.4）
// -----------------------------------------------------------------------------
enum class TextEncoding {
    UTF8 = 0,
    UTF8BOM,
    ShiftJIS,
    UTF16LE,
    Unknown
};

// -----------------------------------------------------------------------------
// 改行コード
// -----------------------------------------------------------------------------
enum class NewlineKind {
    LF = 0,     // \n
    CRLF,       // \r\n
    CR          // \r
};

// 対象種別（設計書 17.2.1 [対象種別]）
enum class TargetKind {
    SingleFile = 0,   // 単一ファイル
    Folder            // フォルダ
};

// -----------------------------------------------------------------------------
// 8.3 検索結果
// -----------------------------------------------------------------------------
struct MatchResult {
    int             match_id = 0;
    std::wstring    file_path;
    size_t          start_offset = 0;
    size_t          end_offset = 0;
    int             start_line = 0;      // 1 起点
    int             end_line = 0;
    int             start_column = 0;    // 1 起点
    int             end_column = 0;
    std::wstring    matched_text;
    MatchType       match_type = MatchType::Exact;
    int             match_rate = 100;    // 類似一致時の一致率(%)
    bool            selected = false;
    MatchStatus     status = MatchStatus::Target;
    std::vector<std::wstring> warning_flags;

    bool HasWarning() const { return !warning_flags.empty(); }
};

// -----------------------------------------------------------------------------
// 8.1 設定情報
// -----------------------------------------------------------------------------
struct AppConfig {
    std::wstring last_target_directory;
    SwitchStyle  default_switch_style = SwitchStyle::IfdefNewFirst;
    bool         create_backup = true;
    bool         create_html_report = true;
    bool         create_log = true;
    bool         open_report_after_execution = false;
    bool         dry_run_only = false;
    bool         auto_indent = false;
    bool         warn_comment_match = false;
    bool         warn_existing_preprocessor = false;
    std::wstring winmerge_path;
    std::wstring default_report_directory;   // 既定: reports
    std::wstring default_backup_directory;   // 既定: backups
    std::wstring encoding_mode;              // auto / utf8 / utf8bom / sjis
    HitBehavior  hit_behavior = HitBehavior::AutoSelectSingle;
    PreviewMode  preview_mode = PreviewMode::TopBottom;
    int          preprocessor_scan_lines = 5;
    std::wstring custom_template;            // 18章

    // ---- 17章 複数ファイル一括処理（Phase 4）----
    TargetKind   target_kind = TargetKind::SingleFile;  // 対象種別
    std::wstring last_target_folder;                    // 前回の対象フォルダ
    std::vector<std::wstring> extension_filter;         // 拡張子フィルタ（例: {L".c", L".h"}）
    bool         include_subdirs = true;                // サブフォルダを含める

    // ---- スイッチ定義ヘッダの一元管理 ----
    bool         manage_switch_header = true;           // スイッチ定義ヘッダを生成/更新する
    bool         add_switch_header_include = true;      // 対象ファイルへ #include を追加する
    std::wstring switch_header_name;                    // 既定: migration_switch.h
    std::wstring switch_header_dir;                     // 空 = 対象と同一フォルダ
    std::wstring switch_memo;                           // スイッチの用途メモ
    std::wstring last_switch_header_path;               // 直近に扱ったヘッダ

    // ---- AI 連携（変更仕様書 / 検査仕様書の生成）----
    int          ai_provider = 0;                       // 0=Genspark API / 1=OpenAI 互換 API
    std::wstring ai_openai_key;                         // OpenAI 互換 API キー
    std::wstring ai_openai_endpoint;                    // 既定: https://api.openai.com/v1/chat/completions
    std::wstring ai_openai_model;                       // 例: gpt-4o-mini
    std::wstring ai_api_key;                            // Genspark API キー
    std::wstring ai_endpoint;                           // 既定: https://www.genspark.ai/api/tool_cli/agent_ask
    std::wstring ai_model;                              // 空 = サーバ既定
    std::wstring ai_task_type;                          // 既定: super_agent
    int          ai_timeout_sec = 600;
    bool         ai_open_report = true;                 // 生成後にレポートを開く
    std::wstring sys_name;                              // システム名
    std::wstring sys_overview;                          // システム概要
    std::wstring sys_purpose;                           // 目的
    std::wstring sys_modification;                      // 修正箇所概要
    std::wstring sys_extra;                             // 追加指示
    std::wstring last_spec_report_path;                 // 直近に生成した仕様書
};

// -----------------------------------------------------------------------------
// 8.2 実行コンテキスト
// -----------------------------------------------------------------------------
struct OperationContext {
    std::wstring  operation_id;
    std::wstring  target_file_path;
    std::wstring  switch_name;
    std::wstring  old_code;
    std::wstring  new_code;
    SwitchStyle   switch_style = SwitchStyle::IfdefNewFirst;
    std::vector<MatchResult> selected_matches;
    ExecutionMode execution_mode = ExecutionMode::Real;
    bool          create_backup = true;
    bool          create_report = true;
    bool          auto_indent = false;
    TextEncoding  encoding = TextEncoding::UTF8;
    NewlineKind   newline = NewlineKind::CRLF;
    std::wstring  started_at;

    // ---- 17章 複数ファイル一括処理（Phase 4）----
    TargetKind    target_kind = TargetKind::SingleFile;
    std::wstring  target_folder;              // 対象フォルダ
    std::vector<std::wstring> extension_filter;
    bool          include_subdirs = true;
    std::vector<std::wstring> target_files;   // 走査で抽出された対象ファイル一覧

    // ---- スイッチ定義ヘッダの一元管理 ----
    bool         manage_switch_header = false;
    bool         add_switch_header_include = false;
    std::wstring switch_header_path;          // ヘッダの絶対パス
    std::wstring switch_memo;                 // スイッチの用途メモ
};

// -----------------------------------------------------------------------------
// 8.4 変換結果
// -----------------------------------------------------------------------------
struct ReplaceResult {
    std::wstring  operation_id;
    std::wstring  target_file_path;
    std::wstring  backup_file_path;
    std::wstring  report_file_path;
    std::wstring  before_text;
    std::wstring  after_text;
    int           replaced_count = 0;
    int           warning_count = 0;
    int           error_count = 0;
    std::wstring  status;          // Success / Failed / Warning
    std::wstring  executed_at;
    std::wstring  switch_header_path;      // 更新したスイッチ定義ヘッダ
    bool          switch_header_updated = false;
    bool          switch_header_added = false;   // スイッチが新規登録されたか
    bool          include_added = false;         // #include を追加したか
    int           include_line = 0;
};

// -----------------------------------------------------------------------------
// 8.5 Undo 履歴
// -----------------------------------------------------------------------------
struct UndoRecord {
    std::wstring operation_id;
    std::wstring target_file_path;
    std::wstring backup_file_path;
    std::wstring report_file_path;
    std::wstring switch_name;
    std::wstring executed_at;
    bool         valid = false;
    // 新規作成されたファイル（例: 新規のスイッチ定義ヘッダ）は Undo で削除して戻す
    bool         remove_on_undo = false;
};

// -----------------------------------------------------------------------------
// ファイル選択時の判定結果（設計書 6.1.3）
// -----------------------------------------------------------------------------
struct FileInfo {
    bool         exists = false;
    bool         readable = false;
    bool         writable = false;
    bool         read_only_attribute = false;
    bool         is_directory = false;
    bool         locked = false;
    unsigned long long size = 0;
    TextEncoding encoding = TextEncoding::Unknown;
    NewlineKind  newline = NewlineKind::CRLF;
    std::wstring newline_string;      // L"\r\n" / L"\n" / L"\r"
    bool         has_bom = false;
};

// -----------------------------------------------------------------------------
// 17章 複数ファイル一括処理（Phase 4）
// -----------------------------------------------------------------------------

// 17.2.2 一括処理: 対象ファイル 1 件分の情報
struct BatchFileEntry {
    std::wstring path;
    std::wstring text;          // LF 正規化済み（ヒットしたファイルのみ保持）
    FileInfo     info;
    int          hit_count = 0;
    int          selected_count = 0;
    bool         selected = true;      // 一括処理の対象に含めるか
    std::wstring status;               // 待機 / 成功 / 失敗 / 変更検出 / 対象外
    std::wstring error;
    bool         include_added = false;   // #include を追加したか
    int          include_line = 0;        // 追加した行番号
};

// 17.2.2 手順7-8: ファイル単位の変換結果
struct FileReplaceResult {
    std::wstring file_path;
    std::wstring backup_file_path;
    int          replaced_count = 0;
    int          warning_count = 0;
    std::wstring status;      // Success / Failed / Skipped
    std::wstring error;
};

// 9章 統合レポート用の一括実行結果
struct BatchExecuteResult {
    std::wstring operation_id;
    std::vector<FileReplaceResult> files;
    int          replaced_count = 0;
    int          warning_count = 0;
    int          error_count = 0;
    int          skipped_count = 0;
    std::wstring status;             // Success / Warning / Failed
    std::wstring executed_at;
    std::wstring report_file_path;
    std::wstring folder;
    std::wstring switch_header_path;    // 更新したスイッチ定義ヘッダ
    bool         switch_header_updated = false;
    bool         switch_header_added = false;   // スイッチが新規追加されたか
};

} // namespace ms

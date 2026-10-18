# 設計書 対応マトリクス

「マイグレーション差し替え支援ツール 詳細設計書」の各章と、本実装の対応箇所を示します。
**○ = 実装済み / △ = 設計どおりオプション扱いまたは簡易実装 / － = 後続版（未実装）**

---

## 1. 機能一覧（設計書 4.1）

| 区分 | 機能名 | 初期版 | 状態 | 実装箇所 |
|---|---|:---:|:---:|---|
| 基本 | 対象ファイル選択 | ○ | ○ | `App::DoBrowse` / `FileManager::Inspect` |
| 基本 | コンパイルスイッチ名入力 | ○ | ○ | `UIManager.cpp` IDC_EDIT_SWITCHNAME / `ValidationManager::ValidateSwitchName` |
| 基本 | 差し替え対象コード入力 | ○ | ○ | IDC_EDIT_OLDCODE / `ValidationManager::ValidateOldCode` |
| 基本 | 新コード入力 | ○ | ○ | IDC_EDIT_NEWCODE / `ValidationManager::ValidateNewCode` |
| 基本 | 完全一致検索 | ○ | ○ | `SearchEngine::Search` |
| 基本 | ヒット件数表示 | ○ | ○ | `App::RefreshListView` / `App::UpdateSelectionColumn` |
| 基本 | ヒット箇所一覧表示 | ○ | ○ | `App::RefreshListView`（ListView 9 カラム） |
| 基本 | プレビュー表示 | ○ | ○ | `App::BuildPreviewInternal` / `PreviewGenerator` |
| 基本 | 本実行 | ○ | ○ | `App::RunOperation(ExecutionMode::Real)` |
| 安全性 | バックアップ作成 | ○ | ○ | `BackupManager::CreateBackup` |
| 安全性 | Undo | ○ | ○ | `UndoManager::Undo`（直前 1 回のみ／6.10.3） |
| 安全性 | ドライラン | ○ | ○ | `App::DoDryRun` → `RunOperation(DryRun)` |
| 安全性 | 上書き確認 | ○ | ○ | `App::Confirm`（既定ボタン = いいえ） |
| 検証 | スイッチ名バリデーション | ○ | ○ | `ValidationManager::ValidateSwitchName` |
| 検証 | ファイル存在確認 | ○ | ○ | `FileManager::Inspect` / `ValidateTargetFile` |
| 検証 | 書き込み権限確認 | ○ | ○ | `FileManager::Inspect`（GENERIC_WRITE オープン試行） |
| 検証 | 空入力チェック | ○ | ○ | `App::CheckInputs` |
| レポート | 差分HTMLレポート出力 | ○ | ○ | `DiffReportGenerator::Generate` |
| レポート | 実行ログ出力 | ○ | ○ | `LogManager::Write` |
| レポート | CSV結果出力 | － | － | 後続版 |
| 高度検索 | 複数ヒット選択 | ○ | ○ | `App::ApplyHitBehavior` / `SyncSelectionFromList`（チェックボックス） |
| 高度検索 | 類似一致検索 | － | － | 後続版（`MatchType::Similar` の器は用意済み） |
| 高度検索 | 指定行範囲検索 | － | － | 後続版 |
| 高度検索 | コメント内除外 | － | △ | 除外ではなく**警告**として実装（`SearchEngine::IsInsideComment`、16章 初期版の簡易判定） |
| 高度検索 | 関数単位検索 | － | － | 後続版 |
| 一括処理 | 複数ファイル対応 | － | ○ | `BatchProcessor::SearchFolder` / `ExecuteBatch` |
| 一括処理 | フォルダ指定 | － | ○ | `FolderScanner::Scan` / `App::DoFolderBrowse` |
| 一括処理 | 拡張子フィルタ | － | ○ | `FolderScanner::MatchesExtension` / 一括処理タブのチェックボックス |
| 外部連携 | WinMerge起動 / レポート出力 | － | － | 後続版（`AppConfig::winmerge_path` は保持済み） |
| 利便性 | 設定保存 | ○ | ○ | `ConfigManager::Load/Save`（config.ini） |
| 利便性 | テンプレート管理 | － | ○ | 18章を前倒し実装（`SwitchGenerator::ExpandTemplate`） |
| 利便性 | インデント自動調整 | － | ○ | 6.8 仕様に沿って前倒し実装（`SwitchGenerator::ApplyAutoIndent`） |
| スイッチ管理 | スイッチ定義ヘッダの一元管理 | － | ○ | `SwitchHeaderManager::Load/Write/RegisterSwitch/SetEnabled/RemoveSwitch` |
| スイッチ管理 | 対象ソースへの `#include` 自動挿入 | － | ○ | `SwitchHeaderManager::EnsureInclude` / `MakeIncludePath` |
| AI 連携 | システム情報の入力（システム名 / 概要 / 目的 / 修正箇所概要） | － | ○ | `UIManager::CreateSystemPage`（システム情報タブ） |
| AI 連携 | 変更仕様書の生成 | － | ○ | `SpecGenerator::BuildChangeSpecPrompt` / `App::GenerateSpecs` |
| AI 連携 | 検査仕様書の生成 | － | ○ | `SpecGenerator::BuildInspectionSpecPrompt` / `App::GenerateSpecs` |
| AI 連携 | API キーの設定（設定タブ） | － | ○ | 基本設定タブ「AI 連携」 / `[Ai] ApiKey` |
| AI 連携 | 仕様書の HTML レポート出力 | － | ○ | `SpecGenerator::WrapReportHtml` / `WriteReportFile` |
| AI 連携 | AI チャット貼り付け用プロンプト txt の出力 | － | ○ | `SpecGenerator::BuildPromptTxt` / `App::ExportPrompts` |
| AI 連携 | 貼り付けた回答のレポート化（API 不使用の経路） | － | ○ | `App::DoMakeReportFromText` / `SpecGenerator::MarkdownToHtml` |

---

## 2. 画面設計（設計書 5章）

| 設計書 | 実装 |
|---|---|
| 5.1 全体レイアウト | `UIManager.cpp: CreateControls / LayoutControls` |
| 5.2 No.1 対象ファイル Edit | IDC_EDIT_TARGETFILE |
| 5.2 No.2 参照 Button | IDC_BTN_BROWSE（`GetOpenFileNameW`） |
| 5.2 No.3 スイッチ名 Edit | IDC_EDIT_SWITCHNAME（`ES_UPPERCASE`） |
| 5.2 No.4 Tab Control | IDC_TAB_MAIN（9 タブ：基本設定／一括処理／旧コード／新コード／検索結果／プレビュー／スイッチ管理／システム情報／ログ） |
| 5.2 No.5/6 旧コード・新コード Multiline Edit | IDC_EDIT_OLDCODE / IDC_EDIT_NEWCODE |
| 5.2 No.7 検索結果 ListView | IDC_LIST_MATCHES（9 カラム・チェックボックス付き） |
| 5.2 No.8 プレビュー ReadOnly Edit | IDC_EDIT_PREVIEW |
| 5.2 No.9 ログ ReadOnly Edit | IDC_EDIT_LOG |
| 5.2 No.10 進捗 Progress Bar | IDC_PROGRESS |
| 5.2 No.11 状態 Status Bar | IDC（STATUSCLASSNAME） |
| 5.2 No.12～16 ボタン | IDC_BTN_SEARCH / PREVIEW / DRYRUN / EXECUTE / UNDO |
| 5.3.1 置換方式 | 方式A～D の 4 ラジオ＋テンプレート入力欄 |
| 5.3.1 オプション | 8 チェックボックス（設計書の全項目） |
| 5.3.1 ヒット時の動作 | 4 ラジオ（`HitBehavior`） |
| 5.4 旧コードタブ | 行数・文字数を常時表示（`App::UpdateInputInfo`） |
| 5.5 新コードタブ | 同上 |
| 5.6.1 ListView カラム | No / 選択 / ファイル名 / 行番号 / 列番号 / 一致種別 / 一致率 / 状態 / プレビュー |
| 5.7.1 プレビュー内容 | 生成スイッチ＋変更前後（`PreviewGenerator`） |
| 5.7.2 表示モード | 単純 / 前後比較 / 左右比較 / HTML表示（コンボボックス） |
| 5.8 ログタブ | `yyyy-mm-dd hh:mm:ss.mmm [LEVEL] message` 形式 |

---

## 3. 機能詳細設計（設計書 6章）

| 節 | 内容 | 実装 |
|---|---|---|
| 6.1 | 対象ファイル選択・各種確認 | `App::DoBrowse` / `FileManager::Inspect` |
| 6.1.4 | エラーメッセージ | `ValidationManager::ValidateTargetFile` |
| 6.2.3 | スイッチ名バリデーション 7 項目 | `ValidateSwitchName`（先頭文字・使用可能文字・空白・ハイフン・記号・予約語警告） |
| 6.3.2 | 旧コードチェック（未入力・空白のみ・行数・文字数・末尾改行） | `ValidateOldCode` / `App::UpdateInputInfo` / `App::DoSearch`（末尾改行の自動調整） |
| 6.4.2 | 新コードチェック（未入力・空白・同一・#ifdef/#endif 混入） | `ValidateNewCode` |
| 6.5.3 | 完全一致検索＋CRLF/CR/LF 正規化＋BOM 除去＋文字コード変換後検索 | `FileManager::ReadText` → `SearchEngine::Search` |
| 6.5.4 | MatchResult 構造 | `Models.h: MatchResult`（match_id / offsets / 行 / 列 / matched_text / status / warning_flags） |
| 6.6 | 複数ヒット処理（0件 / 1件 / 2件以上） | `App::DoSearch` / `ApplyHitBehavior` |
| 6.7.2～6.7.5 | 変換方式 A～D | `SwitchGenerator::Generate` |
| 6.7 補足 | 行中一致の行頭ホイスト（`#ifdef` を行頭へ移動し前後コードを各分岐へ複製） | `PreviewGenerator::BuildAfterTextEach` / `NeedsHoist` / `HoistBlock` |
| 6.8 | インデント自動調整と 6.8.3 の警告 3 種 | `SwitchGenerator::ApplyAutoIndent` |
| 6.9 | バックアップ（`名前.yyyymmdd_hhmmss.bak`、同一フォルダ既定） | `BackupManager::CreateBackup` |
| 6.10 | Undo（直前 1 回、`.undobak` に退避） | `UndoManager::Undo` |
| 6.11 | ドライラン（表の○×どおり） | `App::RunOperation(DryRun)` |
| 6.12.3 | HTML レポートの全項目 | `DiffReportGenerator::BuildHtml` |
| 6.12.5 | 差分色分け 5 種 | `DiffReportGenerator` の CSS（値まで設計書どおり） |
| 6.13 | WinMerge 連携 | － 後続版 |
| 6.14 | 実行ログ（`logs/yyyyMMdd.log`、5 レベル、ローテーション） | `LogManager` |

---

## 4. 内部設計・データ設計（設計書 7・8章）

| 設計書のモジュール | 実装ファイル | 状態 |
|---|---|---|
| UIManager | `UIManager.cpp` | ○ |
| FileManager | `FileManager.h/.cpp` | ○ |
| EncodingDetector | `EncodingDetector.h/.cpp` | ○ |
| SearchEngine | `SearchEngine.h/.cpp` | ○ |
| SwitchGenerator | `SwitchGenerator.h/.cpp` | ○ |
| PreviewGenerator | `PreviewGenerator.h/.cpp` | ○ |
| DiffReportGenerator | `DiffReportGenerator.h/.cpp` | ○ |
| BackupManager | `BackupManager.h/.cpp` | ○ |
| UndoManager | `UndoManager.h/.cpp` | ○ |
| LogManager | `LogManager.h/.cpp` | ○ |
| ConfigManager | `ConfigManager.h/.cpp` | ○ |
| ValidationManager | `ValidationManager.h/.cpp` | ○ |
| SwitchHeaderManager（Phase 5 追加） | `SwitchHeaderManager.h/.cpp` | ○ |
| AiClient（Phase 6 追加） | `AiClient.h/.cpp` | ○ |
| SpecGenerator（Phase 6 追加） | `SpecGenerator.h/.cpp` | ○ |
| （共通）Application | `App.h` / `App.cpp` / `main.cpp` | ○ |

`AppConfig` / `OperationContext` / `MatchResult` / `ReplaceResult` / `UndoRecord` は
設計書 8.1～8.5 の全フィールドを `Models.h` に実装しています
（`match_text` は 6.5.4 の表記、`matched_text` は 8.3 の表記。実装は `matched_text` に統一）。

---

## 5. 処理フロー（設計書 9章）

| 節 | 実装 |
|---|---|
| 9.1 アプリ起動時 | `main.cpp` → `App::Initialize` → `OnCreate`（共通コントロール初期化→設定読込→ウィンドウ生成→設定反映→ログ開始→READY） |
| 9.2 検索処理 | `App::DoSearch` |
| 9.3 プレビュー処理 | `App::BuildPreviewInternal` |
| 9.4 本実行処理（14 手順） | `App::RunOperation(Real)` |
| 9.5 ドライラン処理 | `App::RunOperation(DryRun)` |
| 9.6 Undo 処理 | `App::DoUndo` |

---

## 6. エラー設計・状態管理（設計書 10・13章）

| 設計書 | 実装 |
|---|---|
| 10.1 エラー分類 9 種 | 入力=`CheckInputs` / ファイル・権限=`FileManager::Inspect` / 検索=MSG005 / 複数ヒット=MSG006 / バックアップ=MSG007（実行中断）/ 書き込み=MSG009（自動復旧）/ レポート=警告のみで継続 / Undo=`UndoManager` |
| 10.2 メッセージ MSG001～MSG010 | 全 10 件を実装（該当箇所にそのまま埋め込み） |
| 13.1 アプリ状態 9 種 | `Models.h: AppState` + `App::SetState` |
| 13.2 ボタン活性制御 | `App::UpdateButtons`（設計書の表どおり。△ は `UndoManager::CanUndo()` で判定） |

---

## 7. 安全性設計（設計書 19章）

| 項目 | 実装 |
|---|---|
| 直接上書きしない | `FileManager::WriteTextAtomic`（`temp/` へ書き出し → サイズ検証 → `MoveFileEx(REPLACE_EXISTING \| WRITE_THROUGH)`） |
| 書き込み前にバックアップ | `App::RunOperation`（失敗時は実行中断） |
| 書き込み後のサイズ・読み込み確認 | `WriteTextAtomic` 内で検証 |
| エラー時はバックアップから復旧 | `App::RunOperation` の復旧処理 |
| 空パス・存在しないパス・ディレクトリ指定の禁止 | `ValidateTargetFile` |
| 読み取り専用ファイルの警告 | `ValidateTargetFile`（実行時は属性を一時解除して復元） |
| システムディレクトリ配下の警告 | `util::IsSystemDirectoryPath` |
| ログにソース全文・機密情報を出さない | `LogManager` は件数・行番号のみ記録。変更前後コードはユーザーが明示指定した HTML レポートのみ |

---

## 8. テスト設計（設計書 20章）との対応

`make test` で 296 項目の自動検証を実行できます（GUI と同じオブジェクトファイルをリンク）。

| 設計書 | テスト |
|---|---|
| 20.1 No.1-2 スイッチ名検証 | 11 項目 |
| 20.1 No.3-5 検索 | 11 項目 |
| 20.1 No.6-7 変換 | 6 項目 |
| 20.1 No.8-9 バックアップ / Undo | 10 項目 |
| 20.1 No.10 レポート | 12 項目 |
| 20.2 結合テスト相当 | 文字コード 3 種 × 改行 3 種の往復一致、選択箇所のみ置換、ドライラン相当、読み取り専用ファイル |
| 20.3 異常系相当 | 存在しないファイル、未入力、不正スイッチ名、ヒットなし、バックアップ不可 |

---

## 8.5 Phase 4 実装（設計書 17章 / 21.4）

| 設計書 | 実装 |
|---|---|
| 17.2.1 [対象種別] 単一ファイル／フォルダ | `App::ReadTargetKindFromUi` / 一括処理タブのラジオボタン |
| 17.2.1 [対象フォルダ] + 参照 | `App::DoFolderBrowse`（`SHBrowseForFolderW`） |
| 17.2.1 [拡張子フィルタ] .c .cpp .h .hpp .cs .java | `FolderScanner::CandidateExtensions` / `ReadExtensionsFromUi` |
| 17.2.1 [x] サブフォルダを含める | `FolderScanner::ScanRecursive`（再帰制御・リンクは辿らない） |
| 17.2.2 手順1 フォルダ配下を走査 | `FolderScanner::Scan`（上限 20,000 件 / 8MB） |
| 17.2.2 手順2 拡張子フィルタで対象抽出 | `FolderScanner::MatchesExtension`（大文字小文字を区別しない） |
| 17.2.2 手順3 各ファイルを検索 | `BatchProcessor::SearchFolder` |
| 17.2.2 手順4 ヒット一覧へ集約 | `MatchResult.file_path` で集約し全体通し番号を付与 |
| 17.2.2 手順5 ユーザーが対象を選択 | 対象ファイル一覧＋検索結果一覧のチェックボックス |
| 17.2.2 手順6 一括プレビュー | `App::BuildBatchPreview`（ファイル別の前後比較、上限 50 ファイル） |
| 17.2.2 手順7 一括バックアップ | `BatchProcessor::ExecuteBatch` 内でファイルごとに `BackupManager::CreateBackup` |
| 17.2.2 手順8 一括変換 | ファイルごとの文字コード・改行・インデントを維持して `FileManager::WriteTextAtomic` |
| 17.2.2 手順9 統合レポート出力 | `DiffReportGenerator::GenerateIntegrated`（サマリ表＋ファイル別詳細） |
| 一括操作の Undo | `UndoManager::SetRecords` / `Undo`（操作内の全ファイルをまとめて復元） |

一括処理固有の安全設計:

- ファイル単位で検索結果を再検証し、検索後に変更されたファイルはスキップ（`BatchFileEntry.status = 変更検出`）
- 1 ファイルの失敗で全体を止めず、結果を `BatchExecuteResult` に集約して統合レポートへ反映
- 走査はシンボリックリンク／ジャンクションを辿らない（無限ループ防止）

## 8.6 Phase 5 実装（スイッチ定義ヘッダの一元管理）

| 設計書 | 実装 |
|---|---|
| スイッチ定義ヘッダの生成・更新 | `SwitchHeaderManager::Load` / `Write` / `RegisterSwitch`（管理領域のみ書き換え） |
| スイッチの有効化・無効化・削除 | `SwitchHeaderManager::SetEnabled` / `RemoveSwitch` / `App::DoSwitchEnable` / `DoSwitchDisable` / `DoSwitchRemove` |
| 対象ソースへの `#include` 挿入 | `SwitchHeaderManager::EnsureInclude`（挿入位置 4 段階・冪等） / `App::ApplyIncludeToText` |
| パス解決（別フォルダ・別ドライブ） | `SwitchHeaderManager::MakeIncludePath` / `ResolveHeaderPath` |
| スイッチ管理タブ（ListView 5 列） | `UIManager::CreateSwitchPage`（No / スイッチ名 / 状態 / 登録元 / 説明） |
| 一括処理との連携 | `BatchProcessor::ExecuteBatch` がファイルごとに `EnsureInclude` を適用 |
| レポートへの反映 | `DiffReportGenerator` がヘッダ情報・`#include` 追加行を出力 |
| Undo | `UndoRecord::remove_on_undo`（新規作成ヘッダは削除で復元） |
| 行中一致のホイスト | `PreviewGenerator::NeedsHoist` / `HoistBlock` |
| 設定の保存 | `config.ini` の `[SwitchHeader]` セクション（`manage_switch_header` / `add_switch_header_include` / `switch_header_name` / `switch_header_dir` / `switch_memo` / `last_switch_header_path`） |

---

## 8.7 Phase 6 実装（AI 連携 / 仕様書生成）

| 項目 | 実装 |
|---|---|
| システム情報タブ（システム名 / システム概要 / 目的 / 修正箇所概要 / 追加指示） | `UIManager::CreateSystemPage` / `App::BuildSpecInput` |
| AI 設定（API キー / エンドポイント / モデル / タイムアウト） | 基本設定タブ「AI 連携」 / `App::ReadAiSettings` / `[Ai]` セクション |
| API 送信（WinHTTP + `X-Api-Key`） | `AiClient::Ask` |
| SSE 受信と `delta` 連結 | `AiClient::ParseSseLine`（`data:` プレフィックス対応 / 完了イベントの `last_message` 採用） |
| JSON 生成・解析（外部ライブラリ非依存） | `AiClient::JsonEscape` / `JsonUnescape` / `JsonFindString` / `JsonFindFirstArrayString` |
| プロンプト生成（章立てを指定） | `SpecGenerator::BuildChangeSpecPrompt` / `BuildInspectionSpecPrompt` |
| Markdown → HTML（見出し / 表 / 箇条書き / 強調 / コードブロック / HTML エスケープ） | `SpecGenerator::MarkdownToHtml` |
| レポート出力（UTF-8 BOM / メタ情報 / AI 草案の注記） | `SpecGenerator::WrapReportHtml` / `WriteReportFile` |
| 生成中の UI 応答維持 | `App::PumpMessages`（受信ループからメッセージポンプ） |
| 設定保存（日本語対応のため新規作成時は UTF-16LE BOM で `config.ini` を作成） | `ConfigManager::Save` |
| テスト | `TestAiClientBasics` / `TestSpecGenerator` / `TestAiConfigRoundtrip` / `TestAiLive`（`--live` 指定時のみ実 API を呼ぶ） |
| API 不使用運用（依頼文 txt → 回答貼り付け → レポート化） | `SpecGenerator::BuildPromptTxt` / `MakePromptFileName` / `ToCrlf`、出力欄（`IDC_EDIT_SPECOUT`）は編集可 |

---

## 8.8 追加対応（走査の独立化 / OpenAI 互換 API）

| 項目 | 設計書の記述 | 実装 |
|---|---|---|
| 走査の前提条件 | 4.1 走査機能（前提条件の明示なし） | 対象フォルダと拡張子フィルタのみ検証。旧コード・新コード・スイッチ名は不要。走査ボタンは常時有効 |
| 検索の前提条件 | 6.2 検索 | 旧コードのみ必須。スイッチ名は検索時は警告、実行時に必須（`App::CheckInputs` の `for_execute` で分岐） |
| AI 連携の API | 6.6 AI 連携（Genspark API 前提） | API 種別を選択可。Genspark API と OpenAI 互換 API（`/chat/completions`）の 2 系統。`AiClient::Ask` の先頭で分岐 |
| リクエスト/応答 | 6.6 | Genspark: SSE（`delta` 連結 + `last_message`）。OpenAI 互換: JSON（`choices[0].message.content` / 旧形式 `text` / `error.message`） |
| 設定の保存 | 8章 設定ファイル | `[Ai] Provider / ApiKey / Endpoint / Model / OpenAiApiKey / OpenAiEndpoint / OpenAiModel`。種別ごとに独立保存し、切替時に入力欄を退避・復元 |
| テスト | 20章 | 通常 316 PASS / 0 FAIL、`--live` 322 PASS / 0 FAIL（OpenAI 互換の URL 補完・本文生成・応答解析・設定往復を追加） |

## 8.9 後続版の追加実装（検索条件 / レポート / エクスポート / WinMerge / 複数世代 Undo / EUC-JP）

| 項目 | 実装 |
|---|---|
| CSV 結果出力 | `ResultExporter`（CSV / Markdown / JSON）。`,` `"` 改行を適切にエスケープ。CSV は UTF-8 BOM 既定（Excel 向け）、JSON は BOM なし UTF-8 |
| 類似一致検索 | `SearchEngine::SearchSimilar` / `SimilarityPercent` / `NormalizeForSimilar`。空白の連続を 1 つへ畳んだうえで 2 文字組の Dice 係数を計算し、しきい値以上を検出。完全一致行は除外し、結果は `MatchStatus::Warning` のため自動選択されない |
| 指定行範囲検索 | `SearchEngine::SearchInRange` / `FilterByLineRange`。終了行 0 は最終行まで |
| 関数単位検索 | `CodeScope::FindFunctionBody` / `MatchParen` / `MatchBrace`。文字列・文字リテラル・コメントを考慮した括弧対応。識別子の一部への誤一致を除外。`SearchEngine::SearchInFunction` から利用 |
| WinMerge 連携 | `App::DoWinMerge` / `FindWinMergePath` / `GetCompareTexts`。変換前後を一時ファイルへ書き出し `/e /u /dl /dr` 付きで起動。`[ExternalTools] WinMergePath` または環境変数から自動探索 |
| 複数世代 Undo | `UndoManager` を世代スタック化（`history_`）。1 操作 = 1 世代、`SetMaxGenerations` で上限（1〜100、既定 10）。`Undo` は直近 1 世代のみ pop するため以前の世代も戻せる |
| Markdown / JSON レポート | `DiffReportGenerator::BuildMarkdown` / `BuildJson` / `GenerateMarkdown` / `GenerateJson` と一括処理版。`App::WriteExtraReportsAfterRun` が実行後に追加出力 |
| EUC-JP 対応 | `TextEncoding::EucJp`、`EncodingDetector::IsValidEucJp` / `IsEucJpAvailable` / `EucJpCodePage`（内蔵テーブル）。自動判定では CP932 と EUC-JP の双方が妥当な場合に半角カタカナの出現数で判別。強制モード `eucjp` にも対応 |
| 設定 | `[Search]` セクション: `SimilarEnabled` / `SimilarThreshold` / `LineRangeEnabled` / `LineFrom` / `LineTo` / `FunctionEnabled` / `FunctionName` / `ReportMarkdown` / `ReportJson` / `UndoMaxGenerations` / `LastExportPath` |
| 画面 | 「検索条件」タブを新設（タブは 10 枚）。「検索結果」タブに CSV / Markdown / JSON 出力ボタン、「プレビュー」タブに WinMerge 比較ボタンを追加 |

## 8.10 既存スイッチ警告の判定精度改善

| 項目 | 実装 |
|---|---|
| 内側判定 | 対象行が既存の条件ブロック（`#if` / `#ifdef` / `#ifndef` 〜 `#endif`）の内側かを depth で判定し、二重ネストの危険が最も高いケースとして先に警告する |
| 近傍判定 | 対象行の前後 `PreprocessorScanLines` 行（既定 5）にある条件指令を検出する。走査行数 0 で無効化できる |
| include guard の除外 | `#ifndef G` / `#define G` … `#endif`（`#if !defined(G)` 形式を含む）でファイル全体を包む形は移行スイッチではないため除外する。`CollectIncludeGuardLines` が最初の条件指令と対応する `#endif`（ファイル最後の条件指令）を特定して除外する |
| メッセージ | 内側か近傍かを区別し、検出した指令の行番号・種別・マクロ名を含める |
| テスト | `TestWarningDetection`（include guard 単独・`#if !defined()` 形式・内側・近傍・走査行数 0・guard と併存する本物のスイッチ） |

## 9. 後続版として残した項目

設計書で「後続版」とされている機能は未実装です。拡張ポイントは以下です。

| 機能 | 追加方針 |
|---|---|
| 類似一致検索 | `SearchEngine::Search` に正規化比較を追加し `MatchType::Normalized` / `Similar` を設定 |
| CSV / Markdown / JSON レポート | `DiffReportGenerator` に出力形式を追加 |
| WinMerge 連携 | `FileManager::MakeTempFilePath` で before/after を出力し `CreateProcess` で起動（`winmerge_path` は設定済み） |
| 複数世代 Undo | `UndoManager` の `record_` を `std::vector<UndoRecord>` へ変更 |
| EUC-JP 対応 | `EncodingDetector::Detect/Decode/Encode` に CP20932 を追加 |

---

## 付録: EUC-JP の扱い

EUC-JP の判定・変換は内蔵テーブル（JIS X 0208 / JIS X 0212、`src/EucJpTable.cpp`）で行います。コードページ 51932 は kernel32 に存在せず（`IsValidCodePage` が 0、`MultiByteToWideChar` は `ERROR_INVALID_PARAMETER` で失敗）、OS のコードページに依存すると実機で EUC-JP を判定できないためです。

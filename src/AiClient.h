// =============================================================================
// AiClient.h
// Genspark API（tool_cli / agent_ask）を WinHTTP で呼び出すクライアント
//   - SSE（Server-Sent Events）の delta を逐次連結する
//   - JSON の生成・解析は外部ライブラリに依存しない最小実装
// =============================================================================
#pragma once

#include <string>
#include <functional>

namespace ms {

// 接続先の API 種別
enum class AiProvider {
    Genspark = 0,     // Genspark API（/api/tool_cli/agent_ask、SSE）
    OpenAiCompat = 1  // OpenAI 互換 API（/chat/completions、Bearer 認証）
};

struct AiSettings {
    AiProvider   provider = AiProvider::Genspark;
    std::wstring api_key;
    std::wstring endpoint;      // 既定: https://www.genspark.ai/api/tool_cli/agent_ask
    std::wstring model;         // 空 = サーバ既定
    std::wstring task_type;     // 既定: super_agent
    int          timeout_sec = 600;
};

struct SseEvent {
    bool         has_delta = false;
    std::wstring delta;
    bool         has_final = false;
    std::wstring final_text;
    bool         has_error = false;
    std::wstring error;
    std::wstring status;
};

class AiClient {
public:
    static std::wstring DefaultEndpoint();
    static std::wstring DefaultOpenAiEndpoint();
    static std::wstring DefaultTaskType();

    // ---- OpenAI 互換 API（GPT / Azure OpenAI / Ollama / 社内プロキシ等）----
    static std::wstring ResolveOpenAiUrl(const std::wstring& endpoint);
    static std::string  BuildOpenAiRequestBody(const AiSettings& st, const std::wstring& prompt);
    static bool         ParseOpenAiResponse(const std::string& json,
                                            std::wstring& out_text,
                                            std::wstring& out_error);

    // プロンプトを送信して応答本文を取得する。
    // on_delta: 受信した断片ごと / on_tick: 待機中（UI のメッセージポンプ用）
    static bool Ask(const AiSettings& st,
                    const std::wstring& prompt,
                    std::wstring& out_text,
                    std::wstring& out_error,
                    const std::function<void(const std::wstring&)>& on_delta =
                        std::function<void(const std::wstring&)>(),
                    const std::function<void()>& on_tick =
                        std::function<void()>());

    // OpenAI 互換 API 版（非ストリーミング）
    static bool AskOpenAiCompat(const AiSettings& st,
                                const std::wstring& prompt,
                                std::wstring& out_text,
                                std::wstring& out_error,
                                const std::function<void()>& on_tick =
                                    std::function<void()>());

    // ---- ユーティリティ（テスト対象）----
    static std::string  Utf8Encode(const std::wstring& s);
    static std::wstring Utf8Decode(const std::string& s);
    static std::wstring JsonEscape(const std::wstring& s);
    static std::wstring JsonUnescape(const std::string& s);
    static bool JsonFindString(const std::string& json, const char* key, std::wstring& out);
    static bool JsonFindFirstArrayString(const std::string& json, const char* key, std::wstring& out);
    static SseEvent ParseSseLine(const std::string& line);
    static std::string BuildRequestBody(const AiSettings& st, const std::wstring& prompt);
};

} // namespace ms

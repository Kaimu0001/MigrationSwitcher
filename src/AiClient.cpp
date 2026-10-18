// =============================================================================
// AiClient.cpp
// =============================================================================
#include "AiClient.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

namespace ms {

std::wstring AiClient::DefaultEndpoint()
{
    return L"https://www.genspark.ai/api/tool_cli/agent_ask";
}

std::wstring AiClient::DefaultOpenAiEndpoint()
{
    return L"https://api.openai.com/v1/chat/completions";
}

std::wstring AiClient::DefaultTaskType()
{
    return L"super_agent";
}

// OpenAI 互換 API の URL を組み立てる。
//   ".../v1"                      -> ".../v1/chat/completions"
//   ".../v1/chat/completions"     -> そのまま
//   Azure のように api-version 付きの完全 URL -> そのまま
std::wstring AiClient::ResolveOpenAiUrl(const std::wstring& endpoint)
{
    std::wstring url = endpoint.empty() ? DefaultOpenAiEndpoint() : endpoint;
    while (!url.empty() && url[url.size() - 1] == L'/') url.erase(url.size() - 1);
    std::wstring lower = url;
    for (size_t i = 0; i < lower.size(); ++i)
        if (lower[i] >= L'A' && lower[i] <= L'Z') lower[i] = (wchar_t)(lower[i] - L'A' + L'a');
    if (lower.find(L"/chat/completions") == std::wstring::npos)
        url += L"/chat/completions";
    return url;
}

std::string AiClient::BuildOpenAiRequestBody(const AiSettings& st, const std::wstring& prompt)
{
    std::wstring b = L"{\"model\":\"";
    b += JsonEscape(st.model);
    b += L"\",\"messages\":[{\"role\":\"user\",\"content\":\"";
    b += JsonEscape(prompt);
    b += L"\"}],\"stream\":false}";
    return Utf8Encode(b);
}

bool AiClient::ParseOpenAiResponse(const std::string& json,
                                   std::wstring& out_text, std::wstring& out_error)
{
    out_text.clear();
    out_error.clear();
    if (json.empty()) { out_error = L"API から空の応答が返りました。"; return false; }

    // {"error":{"message":"..."}} 形式
    if (json.find("\"error\"") != std::string::npos) {
        std::wstring msg;
        if (JsonFindString(json, "message", msg) && !msg.empty()) {
            out_error = msg;
            return false;
        }
    }
    // 通常: choices[0].message.content
    std::wstring content;
    if (JsonFindString(json, "content", content) && !content.empty()) {
        out_text = content;
        return true;
    }
    // 旧 completions 形式: choices[0].text
    std::wstring legacy;
    if (JsonFindString(json, "text", legacy) && !legacy.empty()) {
        out_text = legacy;
        return true;
    }
    out_error = L"API の応答から本文を取り出せませんでした。";
    return false;
}

// -----------------------------------------------------------------------------
// UTF-8 / JSON
// -----------------------------------------------------------------------------
std::string AiClient::Utf8Encode(const std::wstring& s)
{
    if (s.empty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                  nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string out;
    out.resize((size_t)n);
    ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

std::wstring AiClient::Utf8Decode(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out;
    out.resize((size_t)n);
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

std::wstring AiClient::JsonEscape(const std::wstring& s)
{
    std::wstring o;
    o.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        switch (c) {
        case L'"':  o += L"\\\""; break;
        case L'\\': o += L"\\\\"; break;
        case L'\n': o += L"\\n";  break;
        case L'\r': o += L"\\r";  break;
        case L'\t': o += L"\\t";  break;
        case L'\b': o += L"\\b";  break;
        case L'\f': o += L"\\f";  break;
        default:
            if (c < 0x20) {
                wchar_t buf[8];
                swprintf(buf, 8, L"\\u%04x", (unsigned)c);
                o += buf;
            } else {
                o += c;
            }
        }
    }
    return o;
}

static void AppendUtf8(std::string& out, unsigned cp)
{
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

static bool Hex4(const std::string& s, size_t pos, unsigned& out)
{
    if (pos + 4 > s.size()) return false;
    unsigned v = 0;
    for (int k = 0; k < 4; ++k) {
        char h = s[pos + (size_t)k];
        v <<= 4;
        if (h >= '0' && h <= '9')      v |= (unsigned)(h - '0');
        else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
        else return false;
    }
    out = v;
    return true;
}

// JSON 文字列リテラルの中身（エスケープ済み）を UTF-8 バイト列へ戻し、
// 最後に UTF-16(wstring) へ変換する。
// ※ 1 バイトずつ wchar_t 化すると日本語が壊れるため、必ずバイト列で組み立てる。
std::wstring AiClient::JsonUnescape(const std::string& s)
{
    std::string out;   // UTF-8
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c != '\\') { out += (char)c; continue; }
        if (i + 1 >= s.size()) break;
        char e = s[++i];
        switch (e) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case '"': out += '"';  break;
        case '\\': out += '\\'; break;
        case '/': out += '/';  break;
        case 'u': {
            unsigned cp = 0;
            if (!Hex4(s, i + 1, cp)) break;
            i += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size() &&
                s[i + 1] == '\\' && s[i + 2] == 'u') {
                unsigned lo = 0;
                if (Hex4(s, i + 3, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
            }
            AppendUtf8(out, cp);
            break;
        }
        default: out += e; break;
        }
    }
    return Utf8Decode(out);
}

// "key" を探し、その値が文字列なら out に入れて true
bool AiClient::JsonFindString(const std::string& json, const char* key, std::wstring& out)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t p = json.find(pat);
    while (p != std::string::npos) {
        size_t q = p + pat.size();
        while (q < json.size() && (json[q] == ' ' || json[q] == '\t')) ++q;
        if (q < json.size() && json[q] == ':') {
            ++q;
            while (q < json.size() && (json[q] == ' ' || json[q] == '\t')) ++q;
            if (q < json.size() && json[q] == '"') {
                ++q;
                std::string raw;
                while (q < json.size() && json[q] != '"') {
                    if (json[q] == '\\' && q + 1 < json.size()) { raw += json[q]; raw += json[q + 1]; q += 2; }
                    else { raw += json[q]; ++q; }
                }
                out = JsonUnescape(raw);
                return true;
            }
        }
        p = json.find(pat, p + 1);
    }
    return false;
}

// "key": [ "first", ... ] の最初の要素を取り出す
bool AiClient::JsonFindFirstArrayString(const std::string& json, const char* key, std::wstring& out)
{
    std::string pat = std::string("\"") + key + "\"";
    size_t p = json.find(pat);
    while (p != std::string::npos) {
        size_t q = p + pat.size();
        while (q < json.size() && (json[q] == ' ' || json[q] == '\t')) ++q;
        if (q < json.size() && json[q] == ':') {
            ++q;
            while (q < json.size() && (json[q] == ' ' || json[q] == '\t')) ++q;
            if (q < json.size() && json[q] == '[') {
                ++q;
                while (q < json.size() && (json[q] == ' ' || json[q] == '\t' || json[q] == '\n')) ++q;
                if (q < json.size() && json[q] == '"') {
                    ++q;
                    std::string raw;
                    while (q < json.size() && json[q] != '"') {
                        if (json[q] == '\\' && q + 1 < json.size()) { raw += json[q]; raw += json[q + 1]; q += 2; }
                        else { raw += json[q]; ++q; }
                    }
                    out = JsonUnescape(raw);
                    return true;
                }
            }
        }
        p = json.find(pat, p + 1);
    }
    return false;
}

SseEvent AiClient::ParseSseLine(const std::string& raw_line)
{
    SseEvent ev;
    std::string line = raw_line;
    while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n'))
        line.erase(line.size() - 1);
    if (line.empty()) return ev;
    if (line.compare(0, 5, "data:") == 0) line = line.substr(5);
    size_t b = line.find_first_not_of(" \t");
    if (b == std::string::npos) return ev;
    line = line.substr(b);
    if (line.empty() || line[0] != '{') return ev;

    std::wstring st;
    if (JsonFindString(line, "status", st)) {
        ev.status = st;
        if (st == L"ok") {
            std::wstring lm;
            if (JsonFindFirstArrayString(line, "last_message", lm)) {
                ev.has_final = true;
                ev.final_text = lm;
            }
        } else if (st != L"processing") {
            std::wstring msg;
            if (JsonFindString(line, "message", msg)) { ev.has_error = true; ev.error = msg; }
            else { ev.has_error = true; ev.error = st; }
        }
    }
    std::wstring d;
    if (JsonFindString(line, "delta", d)) {
        ev.has_delta = true;
        ev.delta = d;
    }
    std::wstring e2;
    if (!ev.has_error && JsonFindString(line, "error", e2) && !e2.empty()) {
        ev.has_error = true;
        ev.error = e2;
    }
    return ev;
}

std::string AiClient::BuildRequestBody(const AiSettings& st, const std::wstring& prompt)
{
    std::wstring body = L"{\"message\":\"";
    body += JsonEscape(prompt);
    body += L"\",\"task_type\":\"";
    body += JsonEscape(st.task_type.empty() ? DefaultTaskType() : st.task_type);
    body += L"\"";
    if (!st.model.empty()) {
        body += L",\"use_model\":\"";
        body += JsonEscape(st.model);
        body += L"\"";
    }
    body += L"}";
    return Utf8Encode(body);
}

// -----------------------------------------------------------------------------
// 実送信（WinHTTP + SSE）
// -----------------------------------------------------------------------------
static bool CrackEndpoint(const std::wstring& url, std::wstring& host, int& port,
                          std::wstring& path, bool& secure)
{
    URL_COMPONENTS uc;
    ::ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t hbuf[256]; hbuf[0] = 0;
    wchar_t pbuf[2048]; pbuf[0] = 0;
    uc.lpszHostName = hbuf;    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = pbuf;     uc.dwUrlPathLength = 2047;

    if (!::WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &uc)) return false;
    host = hbuf;
    path = pbuf;
    secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    port = (int)uc.nPort;
    if (path.empty()) path = L"/";
    return !host.empty();
}

// JSON を POST して応答本文（UTF-8 文字列）を丸ごと受け取る（非ストリーミング）
static bool HttpPostJson(const std::wstring& url, const std::wstring& extra_headers,
                         const std::string& body, std::wstring& out_body,
                         std::wstring& out_error, int timeout_sec,
                         const std::function<void()>& on_tick)
{
    std::wstring host, path;
    int  port = 0;
    bool secure = true;
    if (!CrackEndpoint(url, host, port, path, secure)) {
        out_error = L"エンドポイントの形式が不正です: " + url;
        return false;
    }

    HINTERNET hSession = ::WinHttpOpen(L"CopyMigrationSwitcher/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { out_error = L"WinHTTP セッションを作成できませんでした。"; return false; }
    int tmo = (timeout_sec > 0 ? timeout_sec : 600) * 1000;
    ::WinHttpSetTimeouts(hSession, 30000, 30000, 30000, tmo);

    HINTERNET hConnect = ::WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        ::WinHttpCloseHandle(hSession);
        out_error = L"接続先に到達できません: " + host;
        return false;
    }
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = ::WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                          nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession);
        out_error = L"HTTP リクエストを作成できませんでした。";
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    headers += extra_headers;

    if (!::WinHttpSendRequest(hReq, headers.c_str(), (DWORD)-1L,
                              (LPVOID)body.data(), (DWORD)body.size(),
                              (DWORD)body.size(), 0) ||
        !::WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD e = ::GetLastError();
        out_error = L"API への送信に失敗しました (WinHTTP error " +
                    std::to_wstring((int)e) + L")";
        ::WinHttpCloseHandle(hReq); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0, slen = sizeof(status);
    ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);

    std::string raw;
    for (;;) {
        if (on_tick) on_tick();
        DWORD avail = 0;
        if (!::WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        std::vector<char> buf(avail + 1);
        DWORD read = 0;
        if (!::WinHttpReadData(hReq, &buf[0], avail, &read) || read == 0) break;
        raw.append(&buf[0], read);
    }
    ::WinHttpCloseHandle(hReq);
    ::WinHttpCloseHandle(hConnect);
    ::WinHttpCloseHandle(hSession);

    out_body = AiClient::Utf8Decode(raw);
    if (status != 0 && status != 200) {
        std::wstring detail;
        std::wstring tmp;
        if (AiClient::ParseOpenAiResponse(raw, tmp, detail) == false && !detail.empty()) {
            out_error = L"API が HTTP " + std::to_wstring((int)status) + L" を返しました。\n" + detail;
        } else {
            out_error = L"API が HTTP " + std::to_wstring((int)status) + L" を返しました。";
        }
        return false;
    }
    return true;
}

bool AiClient::AskOpenAiCompat(const AiSettings& st, const std::wstring& prompt,
                               std::wstring& out_text, std::wstring& out_error,
                               const std::function<void()>& on_tick)
{
    out_text.clear();
    out_error.clear();
    if (st.api_key.empty()) {
        out_error = L"API キーが設定されていません。「基本設定」タブの AI 連携で入力してください。";
        return false;
    }
    if (st.model.empty()) {
        out_error = L"モデル名が設定されていません。「基本設定」タブの AI 連携で入力してください。\n"
                    L"（例: gpt-4o-mini / gpt-4o / 使用するサービスのモデル名）";
        return false;
    }

    std::wstring url = ResolveOpenAiUrl(st.endpoint);
    std::wstring headers = L"Authorization: Bearer " + st.api_key + L"\r\n";
    std::string  body = BuildOpenAiRequestBody(st, prompt);

    std::wstring raw;
    if (!HttpPostJson(url, headers, body, raw, out_error, st.timeout_sec, on_tick))
        return false;

    std::string json = Utf8Encode(raw);
    return ParseOpenAiResponse(json, out_text, out_error);
}

bool AiClient::Ask(const AiSettings& st,
                   const std::wstring& prompt,
                   std::wstring& out_text,
                   std::wstring& out_error,
                   const std::function<void(const std::wstring&)>& on_delta,
                   const std::function<void()>& on_tick)
{
    out_text.clear();
    out_error.clear();

    // OpenAI 互換 API は別経路（Bearer 認証 / chat/completions / 非ストリーミング）
    if (st.provider == AiProvider::OpenAiCompat)
        return AskOpenAiCompat(st, prompt, out_text, out_error, on_tick);

    if (st.api_key.empty()) {
        out_error = L"API キーが設定されていません。「基本設定」タブで入力してください。";
        return false;
    }
    std::wstring url = st.endpoint.empty() ? DefaultEndpoint() : st.endpoint;

    std::wstring host, path;
    int  port = 0;
    bool secure = true;
    if (!CrackEndpoint(url, host, port, path, secure)) {
        out_error = L"エンドポイントの形式が不正です: " + url;
        return false;
    }

    HINTERNET hSession = ::WinHttpOpen(L"CopyMigrationSwitcher/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        out_error = L"WinHTTP セッションを作成できませんでした。";
        return false;
    }
    int tmo = (st.timeout_sec > 0 ? st.timeout_sec : 600) * 1000;
    ::WinHttpSetTimeouts(hSession, 30000, 30000, 30000, tmo);

    HINTERNET hConnect = ::WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        ::WinHttpCloseHandle(hSession);
        out_error = L"接続先に到達できません: " + host;
        return false;
    }

    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = ::WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                          nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
        out_error = L"HTTP リクエストを作成できませんでした。";
        return false;
    }

    std::string body = BuildRequestBody(st, prompt);
    std::wstring headers = L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    headers += L"X-Api-Key: " + st.api_key + L"\r\n";

    BOOL ok = ::WinHttpSendRequest(hReq, headers.c_str(), (DWORD)-1L,
                                   (LPVOID)body.data(), (DWORD)body.size(),
                                   (DWORD)body.size(), 0);
    if (!ok) {
        DWORD e = ::GetLastError();
        out_error = L"送信に失敗しました (WinHTTP error " + std::to_wstring((int)e) + L")";
        ::WinHttpCloseHandle(hReq); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession);
        return false;
    }
    if (!::WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD e = ::GetLastError();
        out_error = L"応答を受信できませんでした (WinHTTP error " + std::to_wstring((int)e) + L")";
        ::WinHttpCloseHandle(hReq); ::WinHttpCloseHandle(hConnect); ::WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0, slen = sizeof(status);
    ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);

    std::string pending;
    std::string raw_body;      // エラー時のメッセージ用（先頭のみ）
    std::wstring final_text;
    bool got_final = false;
    bool got_error = false;

    for (;;) {
        if (on_tick) on_tick();
        DWORD avail = 0;
        if (!::WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        std::vector<char> buf(avail + 1);
        DWORD read = 0;
        if (!::WinHttpReadData(hReq, &buf[0], avail, &read) || read == 0) break;
        pending.append(&buf[0], read);
        if (raw_body.size() < 2000) {
            size_t take = read;
            if (raw_body.size() + take > 2000) take = 2000 - raw_body.size();
            raw_body.append(&buf[0], take);
        }

        for (;;) {
            size_t nl = pending.find('\n');
            if (nl == std::string::npos) break;
            std::string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            SseEvent ev = ParseSseLine(line);
            if (ev.has_delta) {
                out_text += ev.delta;
                if (on_delta) on_delta(ev.delta);
            }
            if (ev.has_final) { final_text = ev.final_text; got_final = true; }
            if (ev.has_error && !got_error) {
                got_error = true;
                out_error = ev.error;
            }
        }
    }
    if (!pending.empty()) {
        SseEvent ev = ParseSseLine(pending);
        if (ev.has_delta) { out_text += ev.delta; if (on_delta) on_delta(ev.delta); }
        if (ev.has_final) { final_text = ev.final_text; got_final = true; }
    }

    ::WinHttpCloseHandle(hReq);
    ::WinHttpCloseHandle(hConnect);
    ::WinHttpCloseHandle(hSession);

    if (status != 0 && status != 200) {
        std::wstring msg = L"API が HTTP " + std::to_wstring((int)status) + L" を返しました。";
        if (!raw_body.empty()) msg += L"\n" + Utf8Decode(raw_body);
        out_error = msg;
        return false;
    }
    if (out_text.empty() && got_final) out_text = final_text;

    if (out_text.empty()) {
        if (!out_error.empty()) return false;
        out_error = L"API から本文が返りませんでした。";
        return false;
    }
    return true;
}

} // namespace ms

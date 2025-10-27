#include "pch.h"
#include <iostream>
#include "include/tp2_net/websocket_winhttp.hpp"
#include <stdexcept>
#include <windows.h>

 #ifndef WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE
// Fallback для старых SDK: см. enum WINHTTP_WEB_SOCKET_BUFFER_TYPE
#define WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(5)
#endif
#ifndef WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE
#define WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(6)
#endif

namespace TP2::net {

    struct UrlParts {
        bool   secure = false;        // https/wss → true
        std::wstring host;            // L"stream.bybit.com"
        INTERNET_PORT port = 0;       // 80/443 по умолчанию
        std::wstring path_query;      // L"/v5/public/spot"
    };
    
    static std::wstring utf8_to_wide(std::string_view s) {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return w;
    }
    
    // Поддержка схем: http, https, ws, wss
    static UrlParts crack_url(std::string_view url) {
        UrlParts out{};
        // 1) схема
        bool secure = false;
        size_t pos = 0;
        auto starts = [&](const char* pfx) {
            size_t L = strlen(pfx);
            return url.size() >= L && _strnicmp(url.data(), pfx, L) == 0;
            };
        if (starts("https://")) { secure = true; pos = 8; }
        else if (starts("http://")) { secure = false; pos = 7; }
        else if (starts("wss://")) { secure = true; pos = 6; }   // ← ВАЖНО
        else if (starts("ws://")) { secure = false; pos = 5; }   // ← ВАЖНО
        else {
            return out; // неизвестная схема → вернём пустое (вызовущий код залогирует bad url)
        }
        
        // 2) host[:port][/<path>?query]
        size_t slash = url.find('/', pos);
        std::string_view hostport = (slash == std::string_view::npos) ? url.substr(pos)
            : url.substr(pos, slash - pos);
        std::string_view path = (slash == std::string_view::npos) ? std::string_view{} : url.substr(slash);
        
        // 2a) host / порт
        INTERNET_PORT port = 0;
        size_t colon = hostport.find(':');
        std::string host_utf8;
        if (colon == std::string_view::npos) {
            host_utf8.assign(hostport);
            port = secure ? 443 : 80;
        }
        else {
            host_utf8.assign(hostport.substr(0, colon));
            std::string_view p = hostport.substr(colon + 1);
            port = static_cast<INTERNET_PORT>(atoi(std::string(p).c_str()));
            if (port == 0) port = secure ? 443 : 80;
        }
        
        // 3) path_query: гарантируем ведущий '/'
        std::string path_utf8;
        if (path.empty()) path_utf8 = "/";
        else if (path.front() != '/') { path_utf8.reserve(path.size() + 1); path_utf8.push_back('/'); path_utf8.append(path); }
        else path_utf8.assign(path);
        
        out.secure = secure;
        out.host = utf8_to_wide(host_utf8);
        out.port = port;
        out.path_query = utf8_to_wide(path_utf8);
        return out;
    }
    
WinWebSocketClient::WinWebSocketClient() {
    hSession_ = WinHttpOpen(L"TP2/WinWebSocketClient",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
}

WinWebSocketClient::~WinWebSocketClient() {
    close();
    if (hSession_) { WinHttpCloseHandle(hSession_); hSession_ = nullptr; }
}

std::wstring WinWebSocketClient::to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

WinWebSocketClient::UrlParts WinWebSocketClient::crack_url(const std::string& url) {
    UrlParts out;
    // 0) Определим, была ли исходно защищённая схема
    const bool src_wss = (url.rfind("wss:", 0) == 0) || (url.rfind("WSS:", 0) == 0);
    
    // 1) Для WinHttpCrackUrl подменим схему: wss/ws → https/http (только для разбора)
    std::string url2 = url;
    if (url2.rfind("wss:", 0) == 0 || url2.rfind("WSS:", 0) == 0)      url2.replace(0, 4, "https:");
    else if (url2.rfind("ws:", 0) == 0 || url2.rfind("WS:", 0) == 0) url2.replace(0, 3, "http:");
    // Мини-нормализация: '\' → '/' и гарантируем 'scheme://'
    for (auto& ch : url2) if (ch == '\\') ch = '/';
    if (auto colon = url2.find(':'); colon != std::string::npos) {
        size_t i = colon + 1, cnt = 0; while (i < url2.size() && url2[i] == '/') { ++cnt; ++i; }
        if (cnt < 2) url2.insert(colon + 1, std::string(2 - cnt, '/'));
    }
    auto wurl = to_wide(url2);
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{};  uc.lpszHostName = host; uc.dwHostNameLength = (DWORD)std::size(host);
    wchar_t path[2048]{}; uc.lpszUrlPath = path; uc.dwUrlPathLength = (DWORD)std::size(path);
    wchar_t extra[2048]{};uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = (DWORD)std::size(extra);
    wchar_t scheme[16]{}; uc.lpszScheme = scheme; uc.dwSchemeLength = (DWORD)std::size(scheme);

    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) return out;
    out.port = uc.nPort;
    // WinHTTP не знает про WSS; для "wss" вернётся INTERNET_SCHEME_UNKNOWN.
    // Считаем secure, если схема равна "https" ИЛИ строка схемы равна "wss".
    bool is_https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    bool is_wss = (uc.lpszScheme && _wcsicmp(uc.lpszScheme, L"wss") == 0);
    out.secure = (is_https || is_wss);
    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring p;
    if (uc.dwUrlPathLength) p.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) p.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (p.empty()) p = L"/";
    out.path_query = std::move(p);
    // Если порт не указан, подставим по умолчанию
    if (out.port == 0) out.port = out.secure ? 443 : 80;
    return out;
}

void WinWebSocketClient::connect(const std::string & url) {
    close();
    if (!hSession_) { std::cerr << "[WS][open][err] no session\n"; return; }

    auto u = crack_url(url);
    if (u.port == 0 || u.host.empty() || u.path_query.empty()) {
        std::cerr << "[WS][open][err] bad url: " << url << "\n";
        return;
    }

    hConnect_ = WinHttpConnect(hSession_, u.host.c_str(), u.port, 0);
    if (!hConnect_) { cleanup(); return; }

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    hRequest_ = WinHttpOpenRequest(hConnect_, L"GET", u.path_query.c_str(),
                                   NULL, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest_) { std::cerr << "[WS][open][err] WinHttpOpenRequest GLE=" << GetLastError() << "\n"; cleanup(); return; }

    //if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) { cleanup(); return; }

    // Для WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET lpBuffer должен быть NULL, размер 0.
    // Анализатор C6387 ругается зря — гасим на этой строке.
    
#pragma warning(push)
#pragma warning(suppress:6387)
    if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        std::cerr << "[WS][open][err] WinHttpSetOption(UPGRADE) GLE=" << GetLastError() << "\n";
        cleanup(); return;
    }
#pragma warning(pop)

    if (!WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        std::cerr << "[WS][open][err] WinHttpSendRequest GLE=" << GetLastError() << "\n";
        cleanup(); return;
    }
    
    if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
        std::cerr << "[WS][open][err] WinHttpReceiveResponse GLE=" << GetLastError() << "\n";
        cleanup(); return;
    }
    
    // Диагностика: статус ответа (ожидаем 101)
    DWORD sc = 0, sz = sizeof(sc);
    if (WinHttpQueryHeaders(hRequest_, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &sc, &sz, NULL))
        std::cout << "[WS][open] status=" << sc << "\n";

    hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
    if (!hWebSocket_) {
        std::cerr << "[WS][open][err] WinHttpWebSocketCompleteUpgrade failed, GLE="
            << GetLastError() << "\n";
        return;
    }
    
    std::cout << "[WS][open] upgrade OK\n";
    WinHttpCloseHandle(hRequest_); hRequest_ = nullptr;
    // запускаем ридер (ровно один раз)
    running_ = true;
    if (reader_.joinable()) reader_.join();
    reader_ = std::thread([this] { reader_loop(); });
}

void WinWebSocketClient::on_message(std::function<void(std::string_view)> cb) {
    cb_ = std::move(cb);
}

void WinWebSocketClient::send(std::string_view text) {
    if (!hWebSocket_) {
        std::cerr << "[WS][send][err] socket is null\n";
        return;
    }

    auto rc = WinHttpWebSocketSend(hWebSocket_,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        (PVOID)text.data(),
        (DWORD)text.size());
    if (rc != 0) {
        std::cerr << "[WS][send][err] WinHttpWebSocketSend=" << rc
            << " GetLastError=" << GetLastError() << "\n";
    }
}

void WinWebSocketClient::reader_loop() {
    std::vector<char> buf(64 * 1024);
    while (running_) {
        DWORD bytes = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE tp = WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE;
        
        auto res = WinHttpWebSocketReceive(hWebSocket_, buf.data(), (DWORD)buf.size(), &bytes, &tp);
        if (res != 0) {
            DWORD le = GetLastError();
            std::cerr << "[WS][recv][err] WinHttpWebSocketReceive=" << res
                << " GetLastError=" << le << "\n";
            break;
        }

        if (tp == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            // Расшифруем причину закрытия (WinHTTP требует reason_len <= 123)
            USHORT status = 0;
            BYTE   reason[256]{};
            DWORD  reason_cap = 123;                 // максимум для control frame
            DWORD  reason_used = 0;
            if (WinHttpWebSocketQueryCloseStatus(
                hWebSocket_, &status, reason, reason_cap, &reason_used) == 0) {
                // Гарантируем NUL-терминатор в разумных пределах
                DWORD safe_len = (reason_used < reason_cap) ? reason_used : (reason_cap - 1);
                reason[safe_len] = 0;
                std::cerr << "[WS][close] status=" << status
                    << " reason=\"" << (const char*)reason << "\"\n";
            }
            else {
                std::cerr << "[WS][close] (no status), GLE=" << GetLastError() << "\n";
            }
            break;
        }

        // Прокидываем и TEXT, и BINARY (некоторые паблик-потоки Bybit идут бинарными кадрами).

        // Диагностика: видим любой приходящий кадр
        std::cout << "[WS][recv] type=" << (int)tp << " bytes=" << bytes << "\n";
        
        // Ответ на ping — иначе некоторые серверы ничего не шлют и быстро закрывают сокет.
        if (tp == WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE) {
            // Control frames: в WinHTTP длина payload для PONG должна быть <= 123.
            DWORD pong_len = (bytes > 123) ? 123 : bytes;
            WinHttpWebSocketSend(hWebSocket_,
                WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE,
                pong_len ? (PVOID)buf.data() : NULL,
                pong_len
            );
            continue;
        }

        if (bytes > 0) {
            switch (tp) {
            case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
                if (cb_) cb_(std::string_view(buf.data(), bytes));
                break;
            default:
                // игнорируем прочие типы
                break;
            }
        }
    }

    if (hWebSocket_) {
        //WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
        // reason length для Close также ограничен (<=123). Передаём пустой reason.
        WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
    }
}

void WinWebSocketClient::close() {
    running_ = false;
    if (hWebSocket_) {
        WinHttpWebSocketClose(hWebSocket_, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);
    }
    if (reader_.joinable()) reader_.join();
    cleanup();
}

void WinWebSocketClient::cleanup() {
    if (hWebSocket_) { WinHttpCloseHandle(hWebSocket_); hWebSocket_ = nullptr; }
    if (hRequest_)   { WinHttpCloseHandle(hRequest_); hRequest_ = nullptr; }
    if (hConnect_)   { WinHttpCloseHandle(hConnect_); hConnect_ = nullptr; }
}

} // namespace TP2::net

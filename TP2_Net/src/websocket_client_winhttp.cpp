#include "pch.h"
#include <iostream>
#include "include/tp2_net/websocket_winhttp.hpp"
#include <stdexcept>

#ifndef NOMINMAX
#define NOMINMAX 1     // не позволяем Windows.h определять макросы min/max
#endif
#include <windows.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <algorithm>
#include <cctype>    // std::isdigit
#include <climits>   // INT_MAX

 #ifndef WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE
// Fallback для старых SDK: см. enum WINHTTP_WEB_SOCKET_BUFFER_TYPE
#define WINHTTP_WEB_SOCKET_PING_BUFFER_TYPE static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(5)
#endif
#ifndef WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE
#define WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE static_cast<WINHTTP_WEB_SOCKET_BUFFER_TYPE>(6)
#endif

namespace TP2::net {

    /*
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
    }*/
    
    // -------- file-local helpers -------------------------------------------------
    namespace {
        // UTF-8 -> UTF-16 with validation; file-static to avoid ODR issues.
        inline std::wstring to_wide_impl(std::string_view s) {
            if (s.empty()) return {};
            if (s.size() > static_cast<size_t>(INT_MAX)) return {};;
            const int in_len = static_cast<int>(s.size());
            int n = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), in_len, nullptr, 0);
            if (n <= 0) return {};
            std::wstring w(static_cast<size_t>(n), L'\0');
            int m = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), in_len, w.data(), n);
            if (m <= 0) return {};
            if (m != n) w.resize(static_cast<size_t>(m));
            return w;
        }

        // case-insensitive префикс для ASCII-схем (http/https/ws/wss)
        inline bool ci_starts_with(const std::string & s, const char* pfx) {
            const size_t n = std::strlen(pfx);
            return s.size() >= n && ::_strnicmp(s.data(), pfx, n) == 0;
        }
    } // namespace
    
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
        return to_wide_impl(s);
    }

    /*
WinWebSocketClient::UrlParts WinWebSocketClient::crack_url_old(const std::string& url) {
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
*/

WinWebSocketClient::UrlParts WinWebSocketClient::crack_url(const std::string& url)
{
    /*
    OLD crack_url (for reference):
    // Определяли только http/https, ws/wss обрабатывались криво и могли не ставить secure/порт.
    // Здесь оставлено как памятка; рабочая реализация ниже.
    */
    UrlParts out{};
    if (url.empty()) return out;
    
    // 1) схема
    
    bool secure = false;
    size_t pos = 0;
    if (ci_starts_with(url, "https://")) { secure = true;  pos = 8; }
    else if (ci_starts_with(url, "http://")) { secure = false; pos = 7; }
    else if (ci_starts_with(url, "wss://")) { secure = true;  pos = 6; } // ВАЖНО: wss → secure
    else if (ci_starts_with(url, "ws://")) { secure = false; pos = 5; } // ВАЖНО: ws  → не secure
    else {
        // неизвестная схема
        return out;
    }
    
    out.secure = secure;
    
    // 2) host[:port] (поддержка IPv6 в квадратных скобках)
    const size_t n = url.size();
    size_t host_beg = pos;
    size_t host_end = std::string::npos;
    INTERNET_PORT port = 0;
    
    if (host_beg < n && url[host_beg] == '[') {
        // IPv6: [fe80::1]
        size_t rb = url.find(']', host_beg + 1);
        if (rb == std::string::npos) return out; // некорректный URL
        host_end = rb + 1; // включает ']'
        if (host_end < n && url[host_end] == ':') {
            // порт после IPv6
            size_t p_beg = host_end + 1;
            size_t p_end = p_beg;
            while (p_end < n && std::isdigit((unsigned char)url[p_end])) ++p_end;
            if (p_end == p_beg) return out; // двоеточие без цифр
            port = static_cast<INTERNET_PORT>(std::stoi(url.substr(p_beg, p_end - p_beg)));
            host_end = p_end;
        }
    }
    else {
        // Обычное имя хоста
        size_t slash = url.find('/', host_beg);
        size_t colon = url.find(':', host_beg);
        if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
            // есть порт
            size_t p_beg = colon + 1;
            size_t p_end = p_beg;
            while (p_end < n && std::isdigit((unsigned char)url[p_end])) ++p_end;
            if (p_end == p_beg) return out; // двоеточие без цифр
            port = static_cast<INTERNET_PORT>(std::stoi(url.substr(p_beg, p_end - p_beg)));
            host_end = p_beg - 1; // позиция ':'
            host_end = colon;     // конец host
            host_end = p_end;     // продвинем для дальнейшего вычисления пути
            // поправим ниже через вычисление path_beg
        }
        else {
            // порта нет
            host_end = (slash == std::string::npos) ? n : slash;
        }
    }
    
    // 3) путь + query
    size_t path_beg = (url.find('/', host_beg) != std::string::npos)? url.find('/', host_beg): n;
    
    // Если мы распарсили явный порт в ветке без IPv6 — path_beg уже найден правильно
    // Если IPv6 — host_end указывает на ']' или конец числа порта
    if (path_beg == n) {
        out.path_query = to_wide("/"); // пустой путь → "/"
    }
    else {
        out.path_query = to_wide(url.substr(path_beg));
    }
    
    // хост (сняв квадратные скобки для IPv6)
    std::string host_str;
    if (url[host_beg] == '[') {
        size_t rb = url.find(']', host_beg + 1);
        host_str = url.substr(host_beg + 1, rb - (host_beg + 1));
    }
    else {
        size_t host_end_real = (url.find(':', host_beg) != std::string::npos &&
            (url.find(':', host_beg) < url.find('/', host_beg)))
            ? url.find(':', host_beg)
            : ((path_beg == n) ? n : path_beg);
        host_str = url.substr(host_beg, host_end_real - host_beg);
    }
    out.host = to_wide(host_str);
    
    // 4) дефолтные порты при их отсутствии
    if (port == 0) {
        port = secure ? 443 : 80;
    }
    out.port = port;
    return out;
}

NetErr WinWebSocketClient::connect(const std::string& url,
    const WebSocketOptions & opt,
    const WebSocketHandlers & h) {
    close();
    if (!hSession_) { 
        std::cerr << "[WS][open][err] no session\n"; 
        return NetErr::Unknown;
    }
    options_ = opt;
    handlers_ = h;

    auto u = crack_url(url);
    if (u.port == 0 || u.host.empty() || u.path_query.empty()) {
        std::cerr << "[WS][open][err] bad url: " << url << "\n";
        return NetErr::BadUrl;
    }

    hConnect_ = WinHttpConnect(hSession_, u.host.c_str(), u.port, 0);
    if (!hConnect_) { cleanup(); return NetErr::Connect; }

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    hRequest_ = WinHttpOpenRequest(hConnect_, L"GET", u.path_query.c_str(),
                                   NULL, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest_) { 
        std::cerr << "[WS][open][err] WinHttpOpenRequest GLE=" << GetLastError() << "\n";
        cleanup();
        return NetErr::Connect;
    }

    //if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) { cleanup(); return; }

    // Для WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET lpBuffer должен быть NULL, размер 0.
    // Анализатор C6387 ругается зря — гасим на этой строке.
    
#pragma warning(push)
#pragma warning(suppress:6387)
    if (!WinHttpSetOption(hRequest_, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0)) {
        std::cerr << "[WS][open][err] WinHttpSetOption(UPGRADE) GLE=" << GetLastError() << "\n";
        cleanup(); 
        return NetErr::Protocol;
    }
#pragma warning(pop)

    // Если wss — зажимаем TLS >= 1.2 (и 1.3, если доступна в SDK)
    if (u.secure) {
        DWORD tls = 0;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
        tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#else
        tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#endif
        WinHttpSetOption(hRequest_, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof(tls));
    }

    if (!WinHttpSendRequest(hRequest_, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        std::cerr << "[WS][open][err] WinHttpSendRequest GLE=" << GetLastError() << "\n";
        cleanup(); 
        return NetErr::Connect;
    }
    
    if (!WinHttpReceiveResponse(hRequest_, nullptr)) {
        std::cerr << "[WS][open][err] WinHttpReceiveResponse GLE=" << GetLastError() << "\n";
        cleanup();
        return NetErr::Protocol;
    }
    
    // Диагностика: статус ответа (ожидаем 101)
    DWORD sc = 0, sz = sizeof(sc);
    if (WinHttpQueryHeaders(hRequest_, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &sc, &sz, NULL))
        std::cout << "[WS][open] status=" << sc << "\n";

    hWebSocket_ = WinHttpWebSocketCompleteUpgrade(hRequest_, 0);
    if (!hWebSocket_) {
        std::cerr << "[WS][open][err] WinHttpWebSocketCompleteUpgrade failed, GLE="
            << GetLastError() << "\n";
        return NetErr::Protocol;
    }
    
    std::cout << "[WS][open] upgrade OK\n";
    WinHttpCloseHandle(hRequest_); hRequest_ = nullptr;
    // запускаем ридер (ровно один раз)
    running_ = true;
    if (reader_.joinable()) reader_.join();
    reader_ = std::thread([this] { reader_loop(); });
    if (handlers_.on_open) handlers_.on_open();
    return NetErr::Ok;
}

void WinWebSocketClient::on_message(OnMsg cb) {
    cb_ = std::move(cb);
}

NetErr WinWebSocketClient::send(std::string_view text) {
    if (!hWebSocket_) {
        std::cerr << "[WS][send][err] socket is null\n";
        return NetErr::Closed;
    }

    auto rc = WinHttpWebSocketSend(hWebSocket_,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        (PVOID)text.data(),
        (DWORD)text.size());
    if (rc != 0) {
        std::cerr << "[WS][send][err] WinHttpWebSocketSend=" << rc
            << " GetLastError=" << GetLastError() << "\n";
        return NetErr::Unknown;
    }
    return NetErr::Ok;
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
            if (handlers_.on_close) {
                // гарантия NUL выше; safe cast
                handlers_.on_close(status, (const char*)reason);
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
            if (handlers_.on_ping) handlers_.on_ping();
            continue;
        }
        if (tp == WINHTTP_WEB_SOCKET_PONG_BUFFER_TYPE) {
            if (handlers_.on_pong) handlers_.on_pong();
            continue;
        }

        if (bytes > 0) {
            switch (tp) {
            case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:
            case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
                if (tp == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ||
                    tp == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) {
                    if (handlers_.on_text) handlers_.on_text(std::string_view(buf.data(), bytes));
                    if (cb_)               cb_(std::string_view(buf.data(), bytes)); // legacy
                }
                else {
                    if (handlers_.on_binary) handlers_.on_binary(buf.data(), bytes);
                }
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

void WinWebSocketClient::close(unsigned short code, std::string_view reason) noexcept {
    running_ = false;
    if (hWebSocket_) {
        const USHORT status = code;
        const DWORD  len = (DWORD)std::min<std::size_t>(reason.size(), 123);
        WinHttpWebSocketClose(hWebSocket_, status,
            len ? (PVOID)reason.data() : NULL, len);
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

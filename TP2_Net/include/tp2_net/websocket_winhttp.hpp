
#pragma once
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#include "include/tp2_net/websocket.hpp"

namespace TP2::net {

class WinWebSocketClient final : public IWebSocket {
public:
    WinWebSocketClient();
    ~WinWebSocketClient() noexcept override;
    
    // Новый контракт (см. websocket.hpp)
    [[nodiscard]] NetErr connect(const std::string & url,
        const WebSocketOptions & opt = {},
        const WebSocketHandlers & h = {}) override;
    [[nodiscard]] NetErr send(std::string_view text) override;
    void on_message(OnMsg cb) override;
    void close(unsigned short code = 1000,
        std::string_view reason = {}) noexcept override;

private:
    struct UrlParts {
        std::wstring host;
        std::wstring path_query;
        INTERNET_PORT port{ 0 };
        bool secure{ false };
    };
    
    static std::wstring to_wide(const std::string & s);
    static UrlParts     crack_url(const std::string & url);
    
    void reader_loop();

void cleanup(); 

private:
    HINTERNET hSession_{ nullptr };
    HINTERNET hConnect_{ nullptr };
    HINTERNET hRequest_{ nullptr };
    HINTERNET hWebSocket_{ nullptr };
    
    // Колбэки/опции нового контракта + legacy шорткат
    WebSocketHandlers handlers_{};
    WebSocketOptions  options_{};
    OnMsg             cb_{};           // legacy on_message(text)
    
    std::thread       reader_{};
    std::atomic<bool> running_{ false };
};

} // namespace TP2::net

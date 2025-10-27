
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
    ~WinWebSocketClient() override;

    void connect(const std::string& url) override;
    void send(std::string_view text) override;
    void on_message(std::function<void(std::string_view)> cb) override;
    void close() override;

private:
    std::function<void(std::string_view)> cb_;
    std::atomic<bool> running_{false};
    std::thread reader_;

    HINTERNET hSession_{nullptr};
    HINTERNET hConnect_{nullptr};
    HINTERNET hRequest_{nullptr};
    HINTERNET hWebSocket_{nullptr};

    struct UrlParts {
        std::wstring host;
        std::wstring path_query;
        INTERNET_PORT port{0};
        bool secure{false};
    };
    static std::wstring to_wide(const std::string& s);
    static UrlParts crack_url(const std::string& url);

    void reader_loop();
    void cleanup();
};

} // namespace TP2::net

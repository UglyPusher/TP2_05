#pragma once
#include <string>
#include <string_view>
#include <functional>

namespace TP2::net {
    /// High-level error code independent of platform APIs.
    enum class NetErr {
        Ok, BadUrl, Connect, Tls, Timeout, Protocol, Closed, Cancelled, Unknown
    };
    
    /// Runtime options that affect client behaviour (no OS types).
    struct WebSocketOptions {
        int         ping_interval_ms{ 15000 };
        int         pong_timeout_ms{ 10000 };
        std::size_t max_message_size{ 8 * 1024 * 1024 };
        std::size_t max_send_queue{ 128 };
        bool        validate_utf8{ true };
    };

    struct WebSocketHandlers {
        std::function<void()>                                  on_open;
        std::function<void(std::string_view)>                  on_text;
        std::function<void(const void* data, std::size_t len)> on_binary;
        std::function<void()>                                  on_ping;
        std::function<void()>                                  on_pong;
        std::function<void(unsigned short code, std::string_view reason)> on_close;
        std::function<void(NetErr ec, std::string msg)>        on_error;
    };

    class IWebSocket {
    public:
        using OnMsg = std::function<void(std::string_view)>; // legacy shorthand for text
        virtual ~IWebSocket() noexcept = default;

        /// Establish connection. Returns NetErr::Ok on success.
        [[nodiscard]] virtual NetErr connect(const std::string& url,
            const WebSocketOptions& opt = {},
            const WebSocketHandlers& h = {}) = 0;

        /// Send a text frame.
        [[nodiscard]] virtual NetErr send(std::string_view text) = 0;

        /// Convenience: set only text callback (legacy API).
        virtual void on_message(OnMsg cb) = 0;


        /// Close with an optional code/reason (1000 = normal).
        virtual void close(unsigned short code = 1000, std::string_view reason = {}) noexcept = 0;
    };

    // Dummy WS (no actual networking)
    class DummyWebSocket final : public IWebSocket {
    public:
        ~DummyWebSocket() noexcept override = default;
        [[nodiscard]] NetErr connect(const std::string & url,
            const WebSocketOptions & opt = {},
            const WebSocketHandlers & h = {}) override;
        [[nodiscard]] NetErr send(std::string_view text) override;
        void on_message(OnMsg cb) override;
        void close(unsigned short code = 1000, std::string_view reason = {}) noexcept override;
    private:
        WebSocketHandlers handlers_{};
        WebSocketOptions  options_{};
        std::string       url_{};
        OnMsg             on_msg_{};   // legacy text-callback storage
    };

} // namespace TP2::net

#pragma once
#include <string>
#include <string_view>
#include <functional>

namespace TP2::net {

    class IWebSocket {
    public:
        using OnMsg = std::function<void(std::string_view)>;
        virtual ~IWebSocket() noexcept = default;
        virtual void connect(const std::string& url) = 0;
        virtual void send(std::string_view data) = 0;
        virtual void on_message(OnMsg cb) = 0;
        virtual void close() = 0;
    };

    // Dummy WS (no actual networking)
    class DummyWebSocket final : public IWebSocket {
    public:
        ~DummyWebSocket() noexcept override = default;
        void connect(const std::string& url) override;
        void send(std::string_view data) override;
        void on_message(OnMsg cb) override;
        void close() override;
    private:
        OnMsg on_msg_{};
        std::string url_{};
    };

} // namespace TP2::net

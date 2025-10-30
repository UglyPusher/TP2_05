#pragma once
#include "exchange.hpp"
#include <memory>
namespace TP2 { namespace net { struct IHttpClient; class IWebSocket; } namespace crypto { class ISigner; } }

namespace TP2::ex {
    class BybitAdapter final : public IExchange {
    public:
        BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                     std::shared_ptr<TP2::net::IWebSocket> ws);
        std::string name() const override { return "bybit"; }

        // Явный коннект публичного WS (spot v5). Ставит общий on_message-роутер.
        // Возвращает true при успехе.
        bool connect_public_ws();

        // Market data
        OrderBook get_orderbook(std::string_view symbol, int depth) override;
        void subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)>) override;
        // Лёгкая проверка стрима трейдов: подписка и проброс "сырого" JSON сообщения.
        // Позже можно заменить на типизированный парсер.
        void subscribe_trades_raw(const std::string & symbol,std::function<void(std::string_view)> on_raw);
    
        // Trading (stub for now)
        OrderId place_order(const OrderSpec&) override;
        void cancel_order(std::string_view symbol, std::string_view exch_order_id) override;
    private:
        std::shared_ptr<TP2::net::IHttpClient> http_;
        std::shared_ptr<TP2::net::IWebSocket> ws_;
        std::function<void(std::string_view)>  on_trades_raw_;
        std::function<void(std::string_view)>  ob_ws_handler_; // обработчик сырого WS под стакан
        bool ws_ready_{ false };
    };
} // namespace msg5::ex

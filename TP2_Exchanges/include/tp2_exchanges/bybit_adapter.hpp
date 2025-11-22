#pragma once
#include "exchange.hpp"
#include <memory>
#include <chrono>
namespace TP2 { namespace net { struct IHttpClient; class IWebSocket; } namespace crypto { class ISigner; } }

namespace TP2::ex {
    class BybitAdapter final : public IExchange {
    public:
        BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                     std::shared_ptr<TP2::net::IWebSocket> ws,
                    std::shared_ptr<TP2::net::IWebSocket> private_ws = nullptr
            );
        std::string name() const override { return "bybit"; }

        // Явный коннект публичного WS (spot v5). Ставит общий on_message-роутер.
        // Возвращает true при успехе.
        bool connect_public_ws();

        struct ObHealth {
            uint64_t last_seq{};
            int64_t  last_ts{};
            uint64_t resyncs{};
            bool     strict{};
        };
        
        ObHealth orderbook_health() const noexcept;

        // Управление жёсткостью контроля последовательности стакана (по умолчанию true)
        void set_orderbook_strict(bool v) noexcept { ob_strict_seq_ = v; }

        // Включить/выключить подробные логи ордербука (по умолчанию off)
        void set_orderbook_debug(bool v) noexcept { ob_debug_ = v; }

        // Управление частотой эмиссии собранного стакана
        void set_orderbook_emit_on_every_delta(bool v) noexcept { ob_emit_each_ = v; }
        void set_orderbook_emit_min_interval_ms(int ms) noexcept { ob_emit_min_interval_ms_ = (ms < 0 ? 0 : ms); }

        // Если ордербук не обновлялся дольше N мс — считаем stale и делаем ресинк (0 = выкл)
        void set_orderbook_stale_timeout_ms(int ms) noexcept { ob_stale_timeout_ms_ = (ms < 0 ? 0 : ms); }



        // Market data
        OrderBook get_orderbook(std::string_view symbol, int depth) override;
        void subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)>) override;
        // Лёгкая проверка стрима трейдов: подписка и проброс "сырого" JSON сообщения.
        // Позже можно заменить на типизированный парсер.
        void subscribe_trades_raw(const std::string & symbol,std::function<void(std::string_view)> on_raw);
    
        // Trading (stub for now)
        OrderId place_order(const OrderSpec&) override;
        void cancel_order(std::string_view symbol, std::string_view exch_order_id) override;

        // Cancel all orders
        void cancel_all_orders(std::string_view symbol) override;

      
    private:

        // Приватный коннект сокета после отправки заявки
        bool connect_private_ws(const std::string& key, const std::string& secret);
        bool private_ws_ready_{ false };


        std::shared_ptr<TP2::net::IHttpClient> http_;
        std::shared_ptr<TP2::net::IWebSocket> ws_; // Обычный сокет
        std::shared_ptr<TP2::net::IWebSocket> private_ws_; // Приватный сокет
        std::function<void(std::string_view)>  on_trades_raw_;
        std::function<void(std::string_view)>  ob_ws_handler_; // обработчик сырого WS под стакан
        bool ws_ready_{ false };
        bool ob_strict_seq_{ true };

        bool ob_debug_{ false };

        bool ob_emit_each_{ true };
        int  ob_emit_min_interval_ms_{ 0 };

        std::atomic<uint64_t> ob_last_seq_{ 0 };
        std::atomic<int64_t>  ob_last_ts_{ 0 };
        std::atomic<uint64_t> ob_resyncs_{ 0 };


        int  ob_stale_timeout_ms_{ 5000 };

        // Для stale-детектора — монотонное время последнего ОБНОВЛЕНИЯ книги
        std::atomic<int64_t> ob_last_update_ms_{ 0 };

        // Для шумопонижения: запоминаем последний best bid/ask и время эмиссии
        double ob_last_best_bid_{ 0.0 }, ob_last_best_ask_{ 0.0 };
        std::chrono::steady_clock::time_point ob_last_emit_{};
        
    };
} // namespace TP2::ex

#pragma once
#include "pch.h"
#include <unordered_map>
#include <string>
#include <atomic>
#include "include/tp2_exchanges/exchange_transport.hpp"

#include "include/tp2_net/websocket.hpp"
#include "include/tp2_net/websocket_winhttp.hpp"
namespace tp2::exchanges {

class BybitTransport final : public IExchangeTransport {
public:
    BybitTransport() = default;
    ~BybitTransport() override = default;

    // lifecycle
    void start() override;
    void stop() override;

    // subscriptions (idempotent)
    void subscribe_orderbook(std::string_view symbol, int depth) override;
    void unsubscribe_orderbook(std::string_view symbol) override;
    void subscribe_trades(std::string_view symbol) override;
    void unsubscribe_trades(std::string_view symbol) override;

    // snapshot
    void request_orderbook_snapshot(std::string_view symbol, int depth, SnapshotCb cb) override;

    // callbacks
    void set_on_ob_delta(OnObDelta cb) override    { on_ob_delta_ = std::move(cb); }
    void set_on_ob_snapshot(OnObSnapshot cb) override { on_ob_snapshot_ = std::move(cb); }
    void set_on_trade(OnTrade cb) override         { on_trade_ = std::move(cb); }

private:
    struct SubKey {
        std::string topic; // e.g. "orderbook.50.BTCUSDT" or "publicTrade.BTCUSDT"
        int refcnt{0};
    };
    // простые таблицы рефкаунтов
    std::unordered_map<std::string, SubKey> subs_;
    std::atomic_bool running_{false};

    // доменные колбэки вверх
    OnObDelta    on_ob_delta_;
    OnObSnapshot on_ob_snapshot_;
    OnTrade      on_trade_;

    // служебное
    static std::string ob_topic(std::string_view symbol, int depth);
    static std::string trades_topic(std::string_view symbol);

    // === WS backend ===
    std::unique_ptr<TP2::net::IWebSocket> ws_;   // WinWebSocketClient
    std::string ws_url_ = "wss://stream.bybit.com/v5/public/linear";
    void ws_send_sub_(std::string_view topic);
    void ws_send_unsub_(std::string_view topic);
    void ws_on_open_();
    void ws_on_text_(std::string_view txt);
    void ws_on_close_(unsigned short code, std::string_view reason);
    void ws_on_error_(TP2::net::NetErr ec, std::string msg);
};

} // namespace tp2::exchanges

#pragma once
#include "pch.h"
#include <unordered_map>
#include <string>
#include <atomic>
#include "include/tp2_exchanges/exchange_transport.hpp"

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
};

} // namespace tp2::exchanges

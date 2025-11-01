#pragma once
#include "pch.h"
#include <functional>
#include <string_view>
#include <span>
#include "include/tp2_exchanges/events.hpp"

namespace tp2::exchanges {

// Интерфейс "тонкого" транспорта биржи: сеть, подписки, парсинг, события.
struct IExchangeTransport {
    using ObDeltaSpan = std::span<const ObDelta>;
    using OnObDelta    = std::function<void(std::string_view symbol, ObDeltaSpan deltas,
                                            std::uint64_t seq, std::int64_t ts_ms)>;
    using OnObSnapshot = std::function<void(std::string_view symbol, const ObSnapshot& snap,
                                            std::uint64_t seq, std::int64_t ts_ms)>;
    using OnTrade      = std::function<void(std::string_view symbol, const Trade& t,
                                            std::int64_t ts_ms)>;

    virtual ~IExchangeTransport() = default;

    // Жизненный цикл транспорта
    virtual void start() = 0;
    virtual void stop()  = 0;

    // Подписки маркет-данных (идемпотентные на уровне транспорта)
    virtual void subscribe_orderbook(std::string_view symbol, int depth) = 0;
    virtual void unsubscribe_orderbook(std::string_view symbol) = 0;
    virtual void subscribe_trades(std::string_view symbol) = 0;
    virtual void unsubscribe_trades(std::string_view symbol) = 0;

    // Snapshot для ресинка по требованию домена
    using SnapshotCb = std::function<void(std::string_view symbol, const ObSnapshot& snap,
                                          std::uint64_t seq, std::int64_t ts_ms)>;
    virtual void request_orderbook_snapshot(std::string_view symbol, int depth,
                                            SnapshotCb cb) = 0;

    // Регистрация колбэков домена (вызываются из потока WS транспорта)
    virtual void set_on_ob_delta(OnObDelta cb) = 0;
    virtual void set_on_ob_snapshot(OnObSnapshot cb) = 0;
    virtual void set_on_trade(OnTrade cb) = 0;
};

} // namespace tp2::exchanges

#include "pch.h"
#include "include/tp2_exchanges/bybit_transport.hpp"
#include <format>

namespace tp2::exchanges {

std::string BybitTransport::ob_topic(std::string_view symbol, int depth) {
    return std::format("orderbook.{}.{}", depth, symbol);
}
std::string BybitTransport::trades_topic(std::string_view symbol) {
    return std::format("publicTrade.{}", symbol);
}

void BybitTransport::start() {
    if (running_.exchange(true)) return;
    // TODO: init WS/REST, connect, set message handlers
    // Логи: [TR:bybit][WS] connected / reconnect/backoff
}

void BybitTransport::stop() {
    if (!running_.exchange(false)) return;
    // TODO: close WS, cleanup
}

void BybitTransport::subscribe_orderbook(std::string_view symbol, int depth) {
    auto t = ob_topic(symbol, depth);
    auto &s = subs_[t];
    if (s.refcnt++ == 0) {
        s.topic = t;
        // TODO: send {"op":"subscribe","args":[t]}
    }
}

void BybitTransport::unsubscribe_orderbook(std::string_view symbol) {
    // depth неизвестна -> найдём по префиксу orderbook.*.<symbol>
    for (auto it = subs_.begin(); it != subs_.end(); ) {
        if (it->second.topic.ends_with(std::string{symbol}) &&
            it->second.topic.rfind("orderbook.", 0) == 0) {
            if (--it->second.refcnt <= 0) {
                // TODO: send {"op":"unsubscribe","args":[it->second.topic]}
                it = subs_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void BybitTransport::subscribe_trades(std::string_view symbol) {
    auto t = trades_topic(symbol);
    auto &s = subs_[t];
    if (s.refcnt++ == 0) {
        s.topic = t;
        // TODO: send {"op":"subscribe","args":[t]}
    }
}

void BybitTransport::unsubscribe_trades(std::string_view symbol) {
    auto t = trades_topic(symbol);
    auto it = subs_.find(t);
    if (it != subs_.end() && --it->second.refcnt <= 0) {
        // TODO: send {"op":"unsubscribe","args":[t]}
        subs_.erase(it);
    }
}

void BybitTransport::request_orderbook_snapshot(std::string_view symbol, int depth, SnapshotCb cb) {
    // TODO: REST /v5/market/orderbook?category=linear&symbol=...&limit=depth
    // Прямо сейчас: вернуть пустой снимок как заглушку (компилируется)
    if (cb) {
        ObSnapshot snap{};
        snap.seq = 0; snap.ts_ms = 0; snap.depth = depth;
        cb(symbol, snap, 0, 0);
    }
}

} // namespace tp2::exchanges

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
    ws_ = std::make_unique<TP2::net::WinWebSocketClient>();
    TP2::net::WebSocketHandlers h{};
    h.on_open  = [this]() { ws_on_open_(); };
    h.on_text  = [this](std::string_view s){ ws_on_text_(s); };
    h.on_close = [this](unsigned short code, std::string_view reason){ ws_on_close_(code, reason); };
    h.on_error = [this](TP2::net::NetErr ec, std::string msg){ ws_on_error_(ec, std::move(msg)); };
    TP2::net::WebSocketOptions opt{};
    (void)ws_->connect(ws_url_, opt, h);
    // Логи: [TR:bybit][WS] connected / reconnect/backoff
}

void BybitTransport::stop() {
    if (!running_.exchange(false)) return;
    if (ws_) { ws_->close(1000, "stop"); ws_.reset(); }
}

void BybitTransport::subscribe_orderbook(std::string_view symbol, int depth) {
    auto t = ob_topic(symbol, depth);
    auto &s = subs_[t];
    if (s.refcnt++ == 0) {
        s.topic = t;
        ws_send_sub_(t);
    }
}

void BybitTransport::unsubscribe_orderbook(std::string_view symbol) {
    // depth неизвестна -> найдём по префиксу orderbook.*.<symbol>
    for (auto it = subs_.begin(); it != subs_.end(); ) {
        if (it->second.topic.ends_with(std::string{symbol}) &&
            it->second.topic.rfind("orderbook.", 0) == 0) {
            if (--it->second.refcnt <= 0) {
                ws_send_unsub_(it->second.topic);
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
        ws_send_sub_(t);
    }
}

void BybitTransport::unsubscribe_trades(std::string_view symbol) {
    auto t = trades_topic(symbol);
    auto it = subs_.find(t);
    if (it != subs_.end() && --it->second.refcnt <= 0) {
        ws_send_unsub_(t);
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

// === WS helpers ===
void BybitTransport::ws_on_open_() {
    for (const auto &kv : subs_) ws_send_sub_(kv.first);
}
void BybitTransport::ws_on_text_(std::string_view txt) {
    if (txt.find("\"op\":\"pong\"") != std::string_view::npos) {
        return;
    }
    // TODO: parse bybit v5 messages and forward to callbacks
}
void BybitTransport::ws_on_close_(unsigned short, std::string_view) {
}
void BybitTransport::ws_on_error_(TP2::net::NetErr, std::string) {
}
void BybitTransport::ws_send_sub_(std::string_view topic) {
    if (!ws_) return;
    std::string msg;
    msg.reserve(64 + topic.size());
    msg += R"({"op":"subscribe","args":[")";
    msg.append(topic);
    msg += R"("]})";
    (void)ws_->send(msg);
}
void BybitTransport::ws_send_unsub_(std::string_view topic) {
    if (!ws_) return;
    std::string msg;
    msg.reserve(64 + topic.size());
    msg += R"({"op":"unsubscribe","args":[")";
    msg.append(topic);
    msg += R"("]})";
    (void)ws_->send(msg);
}

} // namespace tp2::exchanges

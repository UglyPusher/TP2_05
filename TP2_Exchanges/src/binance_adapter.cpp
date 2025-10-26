#include "pch.h"
#include "include/tp2_exchanges/binance_adapter.hpp"
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/orderbook_assembler.hpp"

namespace TP2::ex {
BinanceAdapter::BinanceAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                               std::shared_ptr<TP2::net::IWebSocket> ws)
: http_(std::move(http)), ws_(std::move(ws)) {}

OrderBook BinanceAdapter::get_orderbook(std::string_view symbol, int depth) {
    (void)symbol; (void)depth;
    return {}; // stub
}
void BinanceAdapter::subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)> cb) {
    auto assembler = std::make_shared<TP2::ex::OrderBookAssembler>();
    assembler->set_depth(depth);
    assembler->reset_from_snapshot(/*seq*/0, /*ts*/0, {}, {});

    ws_->on_message([assembler, cb](std::string_view /*msg*/) {
        TP2::ex::OrderBookAssembler::Delta d{};
        d.is_bid = true;
        d.seq = assembler->snapshot().seq + 1;
        d.ts = assembler->snapshot().ts + 1;
        d.p = 100.0;
        d.q = 1.0;
        if (!assembler->apply(d)) {
            assembler->mark_unhealthy();
            return;
        }
        cb(assembler->snapshot());
        });
}

OrderId BinanceAdapter::place_order(const OrderSpec&) { return {"binance","N/A",""}; }
void BinanceAdapter::cancel_order(std::string_view, std::string_view) {}
} // namespace msg5::ex

#include "pch.h"
#include "include/tp2_exchanges/sim_adapter.hpp"
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"

namespace TP2::ex {
SimExchangeAdapter::SimExchangeAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                                       std::shared_ptr<TP2::net::IWebSocket> ws)
: http_(std::move(http)), ws_(std::move(ws)) {}

OrderBook SimExchangeAdapter::get_orderbook(std::string_view symbol, int depth) {
    (void)symbol; (void)depth;
    return {}; // stub
}
void SimExchangeAdapter::subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)> cb) {
    (void)symbol; (void)depth;
    ws_->on_message([cb](std::string_view msg){
        (void)msg;
        cb(OrderBook{});
    });
}
OrderId SimExchangeAdapter::place_order(const OrderSpec&) { return {"sim","N/A",""}; }
void SimExchangeAdapter::cancel_order(std::string_view, std::string_view) {}
} // namespace msg5::ex

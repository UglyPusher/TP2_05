// bybit_adapter.cpp
// V5 spot WS: wss://stream.bybit.com/v5/public/spot
// topics: orderbook.<1|50>.<SYMBOL>, publicTrade.<SYMBOL>

#include "pch.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

#include "include/tp2_exchanges/bybit_adapter.hpp"
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/exchange.hpp"
#include "include/tp2_exchanges/orderbook_assembler.hpp"
#include "include/tp2_exchanges/parsers/bybit_v5.hpp"

namespace TP2::ex {

    BybitAdapter::BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
        std::shared_ptr<TP2::net::IWebSocket> ws)
        : http_(std::move(http)), ws_(std::move(ws)) {
    }

    bool BybitAdapter::connect_public_ws() {
        // Ставим единый роутер сообщений
        ws_->on_message([this](std::string_view msg) {
            // 1) трейды: проверяем префикс топика
            if (msg.find("\"topic\":\"publicTrade.") != std::string_view::npos) {
                if (on_trades_raw_) on_trades_raw_(msg);
                return;
            }
            // 2) всё остальное отдаём обработчику стакана (если назначен)
            if (ob_ws_handler_) ob_ws_handler_(msg);
            });

        auto rc = ws_->connect("wss://stream.bybit.com/v5/public/spot");
        if (rc != TP2::net::NetErr::Ok) {
            std::cerr << "[bybit][WS] connect failed\n";
            return false;
        }
        
        ws_ready_ = true;
        std::cout << "[bybit][WS] connected (public spot)\n";
        return true;
    }

    OrderBook BybitAdapter::get_orderbook(std::string_view symbol, int depth) {
        // REST V5: /v5/market/orderbook?category=spot&symbol=BTCUSDT&limit=50
        std::string url = "https://api.bybit.com/v5/market/orderbook?category=spot&symbol=";
        url.append(symbol);
        url.append("&limit=");
        url.append(std::to_string(std::clamp(depth, 1, 200)));

        TP2::net::HttpRequest req;
        req.method = "GET";
        req.url = url;
        auto resp = http_->send(req);

        TP2::ex::parse::Book pb;
        if (!TP2::ex::parse::bybit_rest(resp.body, pb, depth)) {
            return {};
        }

        OrderBook ob;
        ob.bids.reserve(pb.bids.size());
        ob.asks.reserve(pb.asks.size());
        for (auto& pl : pb.bids) ob.bids.push_back({ pl.p, pl.q });
        for (auto& pl : pb.asks) ob.asks.push_back({ pl.p, pl.q });
        ob.seq = pb.seq;
        ob.ts = pb.ts;
        return ob;
    }

    void BybitAdapter::subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)> cb) {
        if (!ws_ready_) {
            std::cerr << "[bybit][WS] not connected; call connect_public_ws() first\n";
            return;
        }
        // Назначаем обработчик сырого WS-фрейма для стакана (простой smoke: каждое сообщение → OrderBook)
        ob_ws_handler_ = [cb, depth](std::string_view msg) {
            TP2::ex::parse::Book pb;
            if (!TP2::ex::parse::bybit_ws(msg, pb, depth)) {
                return; // не сообщение книги — игнорируем
            }
            OrderBook ob;
            ob.seq = pb.seq;
            ob.ts = pb.ts;
            ob.bids.reserve(pb.bids.size());
            ob.asks.reserve(pb.asks.size());
            for (auto& pl : pb.bids) ob.bids.push_back({ pl.p, pl.q });
            for (auto& pl : pb.asks) ob.asks.push_back({ pl.p, pl.q });
            cb(ob);
        };
        // Отправляем подписку на стакан
        int ws_depth = (depth <= 1) ? 1 : 50;
        std::string sub = std::string("{\"op\":\"subscribe\",\"args\":[\"orderbook.")
            +std::to_string(ws_depth) + "." + std::string(symbol) + "\"]}";
        (void)ws_->send(sub);
        std::cout << "[bybit][WS] subscribed: orderbook." << ws_depth << "." << symbol << "\n";
    }

    void BybitAdapter::subscribe_trades_raw(const std::string& symbol,
        std::function<void(std::string_view)> on_raw) {
        on_trades_raw_ = std::move(on_raw);
        if (!ws_ready_) {
            std::cerr << "[bybit][WS] not connected; call connect_public_ws() first\n";
            return;
        }
        // В любом случае — отправляем подписку на трейды.
            std::string subTrades = std::string("{\"op\":\"subscribe\",\"args\":[\"publicTrade.")
            + symbol + "\"]}";
        (void)ws_->send(subTrades);
        std::cout << "[bybit][WS] subscribed: publicTrade." << symbol << "\n";
    }

    OrderId BybitAdapter::place_order(const OrderSpec&) { return { "bybit","N/A","" }; }

    void BybitAdapter::cancel_order(std::string_view, std::string_view) {}

} // namespace TP2::ex

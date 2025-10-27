#include "pch.h"
#include <iostream>
#include <fstream>
#include <string>

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

    OrderBook BybitAdapter::get_orderbook(std::string_view symbol, int depth) {
        // REST V5: /v5/market/orderbook?category=spot&symbol=BTCUSDT&limit=50
        // NB: DummyHttpClient  ;    (  ).
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
        auto assembler = std::make_shared<OrderBookAssembler>();
        assembler->set_depth(depth);
        std::string sym(symbol);
        std::atomic<bool> syncing{ false };

        // 1)    REST
        {
            auto snap = get_orderbook(symbol, depth);
            assembler->reset_from_snapshot(snap.seq, snap.ts, snap.bids, snap.asks);
            if (snap.seq || !snap.bids.empty() || !snap.asks.empty()) {
                cb(assembler->snapshot());
            }
        }

        // 2)   WS:  Bybit V5  { "topic":"orderbook.50.SYMBOL", "type":"snapshot|delta", "ts":..., "data":{ "u":..., "b":[...], "a":[...] } }
        //    WS ( dummy no-op,      URL )
        // : wss://stream.bybit.com/v5/public/spot
        ws_->connect("wss://stream.bybit.com/v5/public/spot");
        //   orderbook.<depth>.<SYMBOL>
        
        // И сразу — подписка (после успешного connect()).
        // Bybit V5: допустимые глубины для orderbook — 1 или 50.        
        {
            int ws_depth = (depth <= 1) ? 1 : 50;
            std::string sub = std::string("{\"op\":\"subscribe\",\"args\":[\"orderbook.")
                +std::to_string(ws_depth)
                +"." + sym + "\"]}";
            ws_->send(sub);
            std::cout << "[bybit][WS] subscribed: orderbook." << ws_depth << "." << sym << "\n";
        }


        // (опц.) Доп. «шумный» топик — удобно проверить приём (идут часто)
        {
            std::string subTrades = std::string("{\"op\":\"subscribe\",\"args\":[\"publicTrade.")
                +sym + "\"]}";
            ws_->send(subTrades);
            std::cout << "[bybit][WS] subscribed: publicTrade." << sym << "\n";
        }
        
        // Пнуть соединение пользовательским ping (Bybit ок с {"op":"ping"})
        ws_->send("{\"op\":\"ping\"}");

        //  :   (  )
        auto do_resync = [this, assembler, &cb, &syncing, sym, depth]() {
            bool expected = false;
            if (!syncing.compare_exchange_strong(expected, true)) return; //   
            auto snap = get_orderbook(sym, depth);
            assembler->reset_from_snapshot(snap.seq, snap.ts, snap.bids, snap.asks);
            cb(assembler->snapshot());
            syncing = false;
            };

        ws_->on_message([assembler, cb, depth, do_resync](std::string_view msg) {
            //   
            std::cout << "[WS] raw " << msg.size() << " bytes\n";
            TP2::ex::parse::Book pb;
            if (!TP2::ex::parse::bybit_ws(msg, pb, depth)) {
                return;
            }

            // heartbeat: Bybit   {"op":"ping"}  {"type":"snapshot|delta",...}
            //      ws_->send("{\"op\":\"pong\"}");

            //      (snapshot  )
            if (pb.is_snapshot) {
                std::vector<OrderBookLevel> bids, asks;
                bids.reserve(pb.bids.size()); asks.reserve(pb.asks.size());
                for (auto& pl : pb.bids) bids.push_back({ pl.p, pl.q });
                for (auto& pl : pb.asks) asks.push_back({ pl.p, pl.q });
                assembler->reset_from_snapshot(pb.seq, pb.ts, std::move(bids), std::move(asks));
                cb(assembler->snapshot());
                return;
            }

            // delta bids
            for (auto& l : pb.bids) {
                OrderBookAssembler::Delta d{};
                d.is_bid = true; d.seq = pb.seq; d.ts = pb.ts; d.p = l.p; d.q = l.q;
                if (!assembler->apply(d)) { do_resync(); return; }
            }

            // delta asks
            for (auto& l : pb.asks) {
                OrderBookAssembler::Delta d{};
                d.is_bid = false; d.seq = pb.seq; d.ts = pb.ts; d.p = l.p; d.q = l.q;
                if (!assembler->apply(d)) { do_resync(); return; }
            }

            cb(assembler->snapshot());
            });
    }

    OrderId BybitAdapter::place_order(const OrderSpec&) { return { "bybit","N/A","" }; }
    void BybitAdapter::cancel_order(std::string_view, std::string_view) {}
} // namespace TP2::ex


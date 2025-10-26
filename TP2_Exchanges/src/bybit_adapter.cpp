#include "pch.h"
#include <cstdlib>
#include "include/tp2_exchanges/bybit_adapter.hpp"
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/exchange.hpp"
#include "include/tp2_exchanges/orderbook_assembler.hpp"

namespace TP2::ex {
BybitAdapter::BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                           std::shared_ptr<TP2::net::IWebSocket> ws)
: http_(std::move(http)), ws_(std::move(ws)) {}

OrderBook BybitAdapter::get_orderbook(std::string_view symbol, int depth) {
    // REST V5: /v5/market/orderbook?category=spot&symbol=BTCUSDT&limit=50
    // NB: DummyHttpClient вернет заглушку; парсер отработает безопасно (вернет пустой стакан).
    std::string url = "https://api.bybit.com/v5/market/orderbook?category=spot&symbol=";
    url.append(symbol);
    url.append("&limit=");
    url.append(std::to_string(std::clamp(depth, 1, 200)));
    
    TP2::net::HttpRequest req;
    req.method = "GET";
    req.url = url;
    auto resp = http_->send(req);
    
    ParsedBook pb;
    if (!parse_rest_snapshot(resp.body, pb, depth)) {
        return {};
    }
    
    OrderBook ob;
    ob.bids = std::move(pb.bids);
    ob.asks = std::move(pb.asks);
    ob.seq = pb.seq;
    ob.ts = pb.ts;
    return ob;
}
void BybitAdapter::subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)> cb) {
    auto assembler = std::make_shared<OrderBookAssembler>();
    assembler->set_depth(depth);
    
    // 1) начальный снапшот через REST
    {
        auto snap = get_orderbook(symbol, depth);
        assembler->reset_from_snapshot(snap.seq, snap.ts, snap.bids, snap.asks);
        if (snap.seq || !snap.bids.empty() || !snap.asks.empty()) {
            cb(assembler->snapshot());
        }
    }
    
    // 2) подписка на WS: ожидаем Bybit V5 формат { "topic":"orderbook.50.SYMBOL", "type":"snapshot|delta", "ts":..., "data":{ "u":..., "b":[...], "a":[...] } }
    ws_->on_message([assembler, cb, depth](std::string_view msg) {
    ParsedBook pb;
    if (!parse_ws_message(msg, pb, depth)) {
        // TODO: логировать непонятные payload, когда включим реальный WS
        // формат не узнали — игнор (или лог)
        return;
    }
    
    // Применяем пачки как последовательность дельт (snapshot просто перезальём)
    if (pb.is_snapshot) {
        assembler->reset_from_snapshot(pb.seq, pb.ts, std::move(pb.bids), std::move(pb.asks));
        cb(assembler->snapshot());
        return;
    }
    
    // delta bids
    for (auto& l : pb.bids) {
        OrderBookAssembler::Delta d{};
        d.is_bid = true; d.seq = pb.seq; d.ts = pb.ts; d.p = l.p; d.q = l.q;
        if (!assembler->apply(d)) { assembler->mark_unhealthy(); return; }
    }
    
    // delta asks
    for (auto& l : pb.asks) {
        OrderBookAssembler::Delta d{};
        d.is_bid = false; d.seq = pb.seq; d.ts = pb.ts; d.p = l.p; d.q = l.q;
        if (!assembler->apply(d)) { assembler->mark_unhealthy(); return; }
    }
    
    cb(assembler->snapshot());
        });
}

OrderId BybitAdapter::place_order(const OrderSpec&) { return {"bybit","N/A",""}; }
void BybitAdapter::cancel_order(std::string_view, std::string_view) {}
} // namespace TP2::ex

// ---------------- Мини-парсер JSON Bybit V5 (b/a, ts, u, type) ----------------
namespace {
    using SV = std::string_view;
    static inline void trim_ws(SV & s) {
        auto issp = [](unsigned char c) { return std::isspace(c) != 0; };
        while (!s.empty() && issp((unsigned char)s.front())) s.remove_prefix(1);
        while (!s.empty() && issp((unsigned char)s.back()))  s.remove_suffix(1);
    }
    
    static inline bool starts_with(SV s, SV p) { return s.substr(0, p.size()) == p; }
    static inline SV after(SV s, SV key) {
        auto pos = s.find(key);
        if (pos == SV::npos) return {};
        return s.substr(pos + key.size());
    }
    
    static bool parse_number(SV s, double& out) {
        trim_ws(s);
        const char* b = s.data(); const char* e = b + s.size();
        // допускаем строки "123.45" или "123" или даже с кавычками
        if (!s.empty() && s.front() == '\"') { ++b; while (e > b && *(e - 1) != '\"') --e; }
        char* endptr = nullptr;
        out = std::strtod(b, &endptr);
        return endptr && endptr > b;
    }
    
    static bool parse_uint64(SV s, uint64_t & out) {
        trim_ws(s);
        const char* b = s.data(); const char* e = b + s.size();
        if (!s.empty() && s.front() == '\"') { ++b; while (e > b && *(e - 1) != '\"') --e; }
        unsigned long long tmp = std::strtoull(b, nullptr, 10);
        out = (uint64_t)tmp;
        return true;
    }
    
    // Извлечь массив пар [[p,q], ...] под ключом key ("\"b\":" или "\"a\":")
    static bool parse_side_levels(SV json, SV key, std::vector<TP2::ex::OrderBookLevel>&out, int limit) {
        out.clear();
        auto tail = after(json, key);
        if (tail.empty()) return false;
        auto lbr = tail.find('[');
        if (lbr == SV::npos) return false;
        int depth = 0;
        size_t i = lbr;
        // ищем внешний массив, затем читать внутренние пары
        for (; i < tail.size(); ++i) { if (tail[i] == '[') { depth++; break; } }
        if (i >= tail.size()) return false;
        ++i; // внутри внешнего массива
        while (i < tail.size() && (int)out.size() < limit) {
            // ищем начало пары
            while (i < tail.size() && (tail[i] != '[' && tail[i] != ']')) ++i;
            if (i >= tail.size()) break;
            if (tail[i] == ']') break; // конец внешнего массива
            // читаем [ price , qty ]
            size_t pair_start = i + 1;
            size_t pair_end = tail.find(']', pair_start);
            if (pair_end == SV::npos) break;
            SV pair = tail.substr(pair_start, pair_end - pair_start);
            // split по запятой
            size_t comma = pair.find(',');
            if (comma == SV::npos) { i = pair_end + 1; continue; }
            SV sp = pair.substr(0, comma);
            SV sq = pair.substr(comma + 1);
            double p = 0, q = 0;
            if (parse_number(sp, p) && parse_number(sq, q)) {
                out.push_back({ p,q });
            }
            i = pair_end + 1;
        }
        return !out.empty();
    }
}

bool TP2::ex::BybitAdapter::parse_ws_message(std::string_view json, ParsedBook & out, int limit_levels) {
    // Ожидаем: "type":"snapshot"|"delta", "ts":<int>, "data":{"u":<seq>,"b":[...],"a":[...]}
    out = {};
    // type
    auto t = after(json, "\"type\"");
    if (t.empty()) return false;
    t = after(t, ":");
    trim_ws(t);
    out.is_snapshot = starts_with(t, "\"snapshot\"");
    // ts
    auto ts_str = after(json, "\"ts\"");
    if (!ts_str.empty()) {
        ts_str = after(ts_str, ":");
        double tsd = 0;
        if (parse_number(ts_str, tsd)) out.ts = static_cast<long long>(tsd);
    }
    
    // data.u
    auto data = after(json, "\"data\"");
    if (!data.empty()) {
        auto u = after(data, "\"u\"");
        if (!u.empty()) {
            u = after(u, ":");
            uint64_t seq = 0;
            if (parse_uint64(u, seq)) out.seq = seq;
        }
    }
    
    // sides
    parse_side_levels(json, "\"b\":", out.bids, limit_levels);
    parse_side_levels(json, "\"a\":", out.asks, limit_levels);
    return true;
}

bool TP2::ex::BybitAdapter::parse_rest_snapshot(std::string_view json, ParsedBook & out, int limit_levels) {
    // Ожидаем: {"retCode":0,"result":{"s":"BTCUSDT","u":<seq>,"ts":<ts>, "b":[...], "a":[...]}}
    out = {};
    auto r = after(json, "\"retCode\"");
    if (r.empty()) return false;
    r = after(r, ":");
    double ret = 1; parse_number(r, ret);
    if (static_cast<int>(ret) != 0) return false;
    auto res = after(json, "\"result\"");
    if (res.empty()) return false;
    // seq
    auto u = after(res, "\"u\"");
    if (!u.empty()) { u = after(u, ":"); uint64_t seq = 0; if (parse_uint64(u, seq)) out.seq = seq; }
    // ts
    auto ts = after(res, "\"ts\"");
    if (!ts.empty()) { ts = after(ts, ":"); double tsd = 0; if (parse_number(ts, tsd)) out.ts = (long long)tsd; }
    // sides
    parse_side_levels(res, "\"b\":", out.bids, limit_levels);
    parse_side_levels(res, "\"a\":", out.asks, limit_levels);
    out.is_snapshot = true;
    return true;
    }

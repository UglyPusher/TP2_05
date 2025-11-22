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
#include <deque>

#include <nlohmann/json.hpp>

#include "include/tp2_exchanges/bybit_adapter.hpp"
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/exchange.hpp"
#include "include/tp2_exchanges/orderbook_assembler.hpp"
#include "include/tp2_exchanges/parsers/bybit_v5.hpp"

#include "include/tp2_crypto/signer.hpp"
#include "include/tp2_crypto/utils.hpp"



namespace TP2::ex {

    BybitAdapter::BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
        std::shared_ptr<TP2::net::IWebSocket> ws, std::shared_ptr<TP2::net::IWebSocket> private_ws)
        : http_(std::move(http)), ws_(std::move(ws)) {
       
    }

    BybitAdapter::ObHealth BybitAdapter::orderbook_health() const noexcept {
        return ObHealth{ ob_last_seq_.load(), ob_last_ts_.load(), ob_resyncs_.load(), ob_strict_seq_ };
    }
    bool BybitAdapter::connect_public_ws() {
        // Ставим единый роутер сообщений
        ws_->on_message([this](std::string_view msg) {
            // лёгкий разбор topic для диагностики маршрутизации
            if (ob_debug_ && msg.find("\"topic\":\"") != std::string_view::npos) {
            auto pos = msg.find("\"topic\":\"");
            auto end = (pos != std::string_view::npos) ? msg.find('"', pos + 9) : std::string_view::npos;
            if (end != std::string_view::npos) {
                auto topic = std::string(msg.substr(pos + 9, end - (pos + 9)));
                std::cout << "[WS][topic] " << topic << "\n";
                }
            }

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


    bool BybitAdapter::connect_private_ws(const std::string& key,
        const std::string& secret)
    {
        const std::string url = "wss://stream.bybit.com/v5/private";

        auto rc = private_ws_->connect(url);
        if (rc != TP2::net::NetErr::Ok) {
            std::cerr << "[bybit][privWS] connect failed\n";
            return false;
        }

        private_ws_ready_ = true;

        std::string ts = TP2::crypto::now_ms_string();
        std::string msg = key + ts;
        std::string sign = TP2::crypto::hmac_sha256_hex(secret, msg);

        nlohmann::json auth;
        auth["op"] = "auth";
        auth["args"] = { key, ts, sign };

        private_ws_->send(auth.dump());

        std::cout << "[bybit][privWS] auth sent\n";

        // подписка на ордера
        nlohmann::json sub;
        sub["op"] = "subscribe";
        sub["args"] = { "order" };
        private_ws_->send(sub.dump());

        std::cout << "[bybit][privWS] subscribed: order\n";

        // логгер всех приватных сообщений
        private_ws_->on_message([](std::string_view raw) {
            std::cout << "[bybit][privWS] " << raw << "\n";
            });

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
        
        // Состояние сборки стакана живёт в лямбде
        auto assembler = std::make_shared<OrderBookAssembler>();
        assembler->set_depth(std::max(1, depth));
        assembler->set_strict(ob_strict_seq_); // управляется извне
        std::string sym(symbol);
        
        // буфер последних N дельт (сырых сообщений) для догонки после REST
        auto recent = std::make_shared<std::deque<std::string>>();
        constexpr size_t kMaxRecent = 64;

        ob_ws_handler_ = [this, cb, depth, assembler, sym, recent](std::string_view msg) {
            TP2::ex::parse::Book pb;
            if (!TP2::ex::parse::bybit_ws(msg, pb, depth)) {
                // Диагностика: почему игнор
                if (!ob_debug_ || msg.find("\"type\"") == std::string_view::npos ||
                    msg.find("\"data\"") == std::string_view::npos) {
                    // это не сообщение книги (ack/ping/др.) — молча выходим
                }
                else {
                    std::cout << "[OB][skip] bybit_ws=false raw=" << msg << "\n";
                }
                return;
            }

            if (ob_debug_) {
                std::cout << "[OB][ws] type=" << (pb.is_snapshot ? "snapshot" : "delta")
                    << " seq=" << pb.seq << " ts=" << pb.ts
                    << " b=" << pb.bids.size() << " a=" << pb.asks.size() << "\n";
            }

            auto should_emit = [&](const OrderBook& ob)->bool {
                if (ob_emit_min_interval_ms_ > 0) {
                    auto now = std::chrono::steady_clock::now();
                    if (ob_last_emit_.time_since_epoch().count() != 0) {
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - ob_last_emit_).count();
                        if (ms < ob_emit_min_interval_ms_) return false;
                    }
                    ob_last_emit_ = now;
                }
                
                if (ob_emit_each_) return true;
                // emit_only_on_top_change: сравним best bid/ask по цене
                double bb = (ob.bids.empty() ? 0.0 : ob.bids.front().p);
                double ba = (ob.asks.empty() ? 0.0 : ob.asks.front().p);
                if (bb != ob_last_best_bid_ || ba != ob_last_best_ask_) {
                    ob_last_best_bid_ = bb; ob_last_best_ask_ = ba; return true;
                }
                return false;
                };

            auto now_ms = []{
                return (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
                };

            auto check_stale_and_resync = [&]() {
                if (ob_stale_timeout_ms_ <= 0) return false;
                int64_t last = ob_last_update_ms_.load();
                if (last > 0 && (now_ms() - last) > ob_stale_timeout_ms_) {
                    if (ob_debug_) std::cerr << "[bybit][OB] stale -> resync via REST\n";
                    OrderBook snap = this->get_orderbook(sym, depth);
                    if (!snap.bids.empty() || !snap.asks.empty()) {
                        std::vector<OrderBookLevel> rb = snap.bids, ra = snap.asks;
                        assembler->reset_from_snapshot(snap.seq, snap.ts, std::move(rb), std::move(ra));
                        ob_last_seq_ = snap.seq; ob_last_ts_ = snap.ts;
                        ob_last_update_ms_ = now_ms();
                        cb(assembler->snapshot());
                        return true;
                    }
                }
                return false;
            };
            
            (void)check_stale_and_resync();
            if (pb.is_snapshot) {
                std::vector<OrderBookLevel> bids; bids.reserve(pb.bids.size());
                std::vector<OrderBookLevel> asks; asks.reserve(pb.asks.size());
                for (auto& pl : pb.bids) bids.push_back({ pl.p, pl.q });
                for (auto& pl : pb.asks) asks.push_back({ pl.p, pl.q });
                
                assembler->reset_from_snapshot(pb.seq, pb.ts, std::move(bids), std::move(asks));
                ob_last_seq_ = pb.seq; ob_last_ts_ = pb.ts;
                ob_last_update_ms_ = now_ms();
                
                { auto ob = assembler->snapshot(); if (should_emit(ob)) cb(ob); }
                return;
            }

            // delta: применяем обе стороны
            OrderBookAssembler::Delta d{};
            d.seq = pb.seq; d.ts = pb.ts;
            
            // сохраняем сырую дельту в кольцевой буфер (для потенциальной догонки)
            if (!pb.is_snapshot) {
                recent->emplace_back(msg);
                if (recent->size() > kMaxRecent) recent->pop_front();
            }
            
            auto resync = [&]() {
                if (ob_debug_) std::cerr << "[bybit][OB] gap detected -> resync via REST\n";
                ob_resyncs_.fetch_add(1);
                OrderBook snap = this->get_orderbook(sym, depth);
                if (snap.bids.empty() && snap.asks.empty()) return; // не удалось ресинкнуться
                std::vector<OrderBookLevel> rb = snap.bids, ra = snap.asks;
                assembler->reset_from_snapshot(snap.seq, snap.ts, std::move(rb), std::move(ra));
                ob_last_seq_ = snap.seq;
                ob_last_ts_ = snap.ts;
                
                // догнать дельтами из буфера, которые новее снапшота
                TP2::ex::parse::Book rbk;
                for (auto& raw : *recent) {
                    if (!TP2::ex::parse::bybit_ws(raw, rbk, depth)) continue;
                    if (rbk.is_snapshot || rbk.seq <= snap.seq) continue;
                    OrderBookAssembler::Delta dd{}; dd.ts = rbk.ts; dd.seq = rbk.seq;
                    for (auto& pl : rbk.bids) { dd.is_bid = true;  dd.p = pl.p; dd.q = pl.q; if (!assembler->apply(dd)) break; }
                    for (auto& pl : rbk.asks) { dd.is_bid = false; dd.p = pl.p; dd.q = pl.q; if (!assembler->apply(dd)) break; }
                }
                
                { auto ob2 = assembler->snapshot(); if (should_emit(ob2)) cb(ob2); }
            };

            for (auto& pl : pb.bids) { d.is_bid = true;  d.p = pl.p; d.q = pl.q; if (!assembler->apply(d)) { resync(); return; } }
            for (auto& pl : pb.asks) { d.is_bid = false; d.p = pl.p; d.q = pl.q; if (!assembler->apply(d)) { resync(); return; } }
            ob_last_seq_ = pb.seq;
            if (pb.ts) ob_last_ts_ = pb.ts;
            ob_last_update_ms_ = now_ms();

            auto ob = assembler->snapshot(); 
            if (should_emit(ob)) cb(ob);
        }; // ob_ws_handler_
            // Отправляем подписку на стакан

        int ws_depth = (depth <= 1) ? 1 : 50;
        std::string sub = std::string("{\"op\":\"subscribe\",\"args\":[\"orderbook.")
            +std::to_string(ws_depth) + "." + std::string(symbol) + "\"]}";
        (void)ws_->send(sub);
        std::cout << "[bybit][WS] subscribed: orderbook." << ws_depth << "." << symbol << "\n";
        // На Bybit v5 часто сразу приходит ack об успешной подписке — посмотрим его в роутере.
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

    OrderId BybitAdapter::place_order(const OrderSpec& spec) { 

        std::string api_key_ = std::string("123123");
        std::string api_secret_ = std::string("123123");

        //Если нет ключа - выкидваем ошибку 
        if (api_key_.empty() || api_secret_.empty()) {
            throw ExchangeError("Bybit place_order: API credentials not set");
        };


     
        nlohmann::json j;



        // Нам нужно еще получать категорию
        j["category"] = "spot";
        j["symbol"] = spec.symbol;

        // может быть Sell or Buy
       // j["side"] = spec.side;
        j["side"] = (spec.side == TP2::ex::Side::Sell) ? 1 : 0;


        switch (spec.type) {
        case OrdType::Market:
            j["orderType"] = "Market";
            break;

        case OrdType::Limit:
            j["orderType"] = "Limit";
            if (!spec.price)
                throw ExchangeError("Limit order requires price");
            j["price"] = std::to_string(*spec.price);
            break;

        case OrdType::Stop:
            j["orderType"] = "Market";
            if (!spec.stop_price)
                throw ExchangeError("Stop order requires stop_price");
            j["triggerPrice"] = std::to_string(*spec.stop_price);

            // Вот тут надо точнее 
            j["triggerBy"] = "LastPrice"; 
            break;

        case OrdType::StopLimit:
            j["orderType"] = "Limit";
            if (!spec.stop_price)
                throw ExchangeError("StopLimit requires stop_price");
            if (!spec.price)
                throw ExchangeError("StopLimit requires price");
            j["price"] = std::to_string(*spec.price);
            j["triggerPrice"] = std::to_string(*spec.stop_price);
            j["triggerBy"] = "LastPrice";
            break;

        case OrdType::PostOnly:
            j["orderType"] = "Limit";
            if (!spec.price)
                throw ExchangeError("PostOnly requires price");
            j["price"] = std::to_string(*spec.price);
            j["timeInForce"] = "PostOnly";
            break;
        default:
            break;
        };

        if (spec.qty)
            j["qty"] = std::to_string(*spec.qty);
        else if (spec.quote_qty)
            // Bybit не поддерживает quoteQty для spot 
            j["qty"] = std::to_string(*spec.quote_qty); 
        else
            throw ExchangeError("Order must have qty");

        if (spec.tif)
            j["timeInForce"] = *spec.tif;

       
        if (spec.client_id)
            j["orderLinkId"] = *spec.client_id;




        nlohmann::json sign_payload;

        std::string body = j.dump();
        std::string method = "POST";
        std::string path = "/v5/order/create";

        sign_payload["secret"] = api_secret_;
        sign_payload["method"] = method;
        sign_payload["path"] = path;
        sign_payload["body"] = body;
        

        TP2::crypto::BybitSigner signer;        
        std::string sign = signer.sign(sign_payload.dump()); 
        


        TP2::net::HttpRequest req;

        req.method = method;

        // Из конфига выдернуть юрл
        req.url = "http://api/bybit/market" + path;

        auto res = http_->send(req);

        // Проверка статуса ответа
        if (res.status != 200) {
            ExchangeError err("Bybit place_order HTTP error");
            err.http_status = res.status;
            err.retryable = res.status >= 500;
            throw err;
        }

        nlohmann::json response;
        try { response = nlohmann::json::parse(res.body); }
        catch (const std::exception& ex) {
            ExchangeError err(std::string("JSON parse error: ") + ex.what());
            err.http_status = res.status;
            throw err;
        }

        // Проверка json поля retCode
        if (response.value("retCode", -9999) != 0) {
            ExchangeError err("Bybit error: " + response.dump());
            err.vendor_code = response.value("retCode", -1);
            err.retryable = false;
            throw err;
        }

        auto result = response["result"];
        OrderId out;
        out.exchange = "bybit";
        out.native_id = response.value("orderId", "");
        out.client_id = response.value("orderLinkId", "");



        // Открываем приватный сокет
        if (!private_ws_ready_) {
            connect_private_ws(api_key_, api_secret_);
        }

        return {
            out.exchange,
            out.native_id,
            out.client_id

        };
    
        //return { "bybit","N/A","" }; 
    
    }

    void BybitAdapter::cancel_order(std::string_view symbol, std::string_view order_id) {

        nlohmann::json j;


        std::string api_secret_ = std::string("");
        std::string api_key_ = std::string("");

        j["category"] = "spot";
        j["symbol"] = symbol;
        j["orderId"] = order_id;  
        std::string body = j.dump();

        std::string path = "/v5/order/cancel";
        std::string method = "POST";

        std::string ts = TP2::crypto::now_ms_string();
        std::string sign = TP2::crypto::bybit_sign(
            api_secret_,
            ts,
            method,
            path,
            body
        );

        TP2::net::HttpRequest req;

        req.method = method;
        req.url = "https://api.bybit.com" + path;


        req.headers["Content-Type"] = "application/json";
        req.headers["X-BAPI-API-KEY"] = api_key_;
        req.headers["X-BAPI-TIMESTAMP"] = ts;
        req.headers["X-BAPI-SIGN"] = sign;
        req.headers["X-BAPI-RECV-WINDOW"] = "5000"; 

        req.body = body;

        auto res = http_->send(req);

        if (res.status != 200) {
            ExchangeError err("Bybit cancel_order HTTP error");
            err.http_status = res.status;
            err.retryable = res.status >= 500;
            throw err;
        }


        nlohmann::json response;
        try { response = nlohmann::json::parse(res.body); }
        catch (const std::exception& ex) {
            ExchangeError err(std::string("JSON parse error: ") + ex.what());
            err.http_status = res.status;
            throw err;
        }

        if (response.value("retCode", -9999) != 0) {
            ExchangeError err("Bybit error: " + response.dump());
            err.vendor_code = response.value("retCode", -1);
            err.retryable = false;
            throw err;
        }

        
      
    }


    void BybitAdapter::cancel_all_orders(std::string_view symbol) {


        // ВЫНЕСТИ В КОНСТРУКТОР КЛАССА ИЛИ КОНФИГ
        std::string api_secret_ = std::string("");
        std::string api_key_ = std::string("");

        nlohmann::json j;

        // Собираем тело запроса
        j["symbol"] = symbol;
        j["category"] = "spot";
        std::string body = j.dump();

        // Путь и метод запроса
        std::string path = "/v5/order/cancel-all";
        std::string method = "POST";
        std::string ts = TP2::crypto::now_ms_string();

        // Создаем подпись
        std::string sign = TP2::crypto::bybit_sign(
            api_secret_,
            ts,
            method,
            path,
            body
        );


        // Подготовка запроса
        TP2::net::HttpRequest req;
        req.method = method;
        req.url = "https://api.bybit.com" + path;


        // Добавляем хедеры
        req.headers["Content-Type"] = "application/json";
        req.headers["X-BAPI-API-KEY"] = api_key_;
        req.headers["X-BAPI-TIMESTAMP"] = ts;
        req.headers["X-BAPI-SIGN"] = sign;
        req.headers["X-BAPI-RECV-WINDOW"] = "5000";

        req.body = body;



        // Отправка запроса
        auto res = http_->send(req);

        // Проверка статуса
        if (res.status != 200) {
            ExchangeError err("Bybit cancel_all_orders HTTP error");
            err.http_status = res.status;
            err.retryable = res.status >= 500;
            throw err;
        }

        // Парсим ответ с сервера
        // TODO: Дописать типы
        nlohmann::json response;
        try { response = nlohmann::json::parse(res.body); }
        catch (const std::exception& ex) {
            ExchangeError err(std::string("JSON parse error: ") + ex.what());
            err.http_status = res.status;
            throw err;
        }


        // Проверка retCode
        if (response.value("retCode", -9999) != 0) {
            ExchangeError err("Bybit cancel_all_orders error: " + response.dump());
            err.vendor_code = response.value("retCode", -1);
            err.retryable = false;
            throw err;
        }
    
    }



} // namespace TP2::ex

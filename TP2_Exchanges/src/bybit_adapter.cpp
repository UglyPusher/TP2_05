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
        : http_(std::move(http)), ws_(std::move(ws)), private_ws_(std::move(private_ws)) {
       
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

    // Вынести в парсеры !!!!!
    void BybitAdapter::handle_private_msg(std::string_view raw) {
    // просто выводим то, что пришло

    const bool ob_debug_ = true;
    if (ob_debug_) {
        std::cout << "[bybit][privWS] recv: " << raw << "\n";
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(raw);
    } catch (...) {
        return;
    }

    // системные сообщения: auth, subscribe, ping
    if (!j.contains("topic")) {
        return;
    }

    std::string topic = j.value("topic", "");

    // ----------------------- ORDER -----------------------
    if (topic == "order") {
        if (on_order_update_) {
            for (auto& ord : j["data"]) {
                on_order_update_(ord);
            }
        }
        return;
    }

    // ---------------------- POSITION ---------------------
    if (topic == "position") {
        if (on_position_update_) {
            for (auto& pos : j["data"]) {
                on_position_update_(pos);
            }
        }
        return;
    }
}

    bool BybitAdapter::connect_private_ws(const std::string& key,
        const std::string& secret)
    {
        const std::string url = "wss://stream.bybit.com/v5/private";
        const std::string testnet = "wss://stream-testnet.bybit.com/v5/private?max_active_time=1m";

        auto rc = private_ws_->connect(testnet);
        if (rc != TP2::net::NetErr::Ok) {
            std::cerr << "[bybit][privWS] connect failed\n";
            return false;
        }

        // Сначала устанавливаем обработчик
        private_ws_->on_message([this](std::string_view raw) {
            this->handle_private_msg(raw);
            });

        private_ws_ready_ = true;

        // Теперь готовим аутентификацию
        std::string expires = TP2::crypto::now_ms_string();
        uint64_t exp = TP2::crypto::now_ms() + 10000;

        std::string sign_message = "GET/realtime" + std::to_string(exp);
        std::string sign = TP2::crypto::hmac_sha256_hex(secret, sign_message);


        nlohmann::json auth;
        auth["op"] = "auth";
        auth["args"] = nlohmann::json::array({ key, exp, sign });

        auto auth_json = auth.dump();
        std::cout << "[bybit][privWS] Auth JSON: " << auth_json << "\n";

        auto ob = private_ws_->send(auth_json);
        std::cout << "[bybit][privWS] auth sent\n";

        return true;
    }

    void BybitAdapter::private_subscribe_position() {
        if (!private_ws_ready_) {
            std::cerr << "[bybit][privWS] not connected; call connect_public_ws() first\n";
            return;
        }

        on_position_update_ = [](const nlohmann::json& ord) {
            std::cout << "[POSITION UPDATE] "
                << ord.value("symbol", "") << " "
                << ord.value("orderStatus", "") << " "
                << ord.value("side", "") << "\n";
            };

        nlohmann::json sub;
        sub["op"] = "subscribe";
        sub["args"] = { "position" };
        auto ob = private_ws_->send(sub.dump());
        std::cout << "[bybit][privWS] subscribed: position\n";
    }


    void BybitAdapter::private_subscribe_order()
    {
        if (!private_ws_ready_) {
            std::cerr << "[bybit][privWS] not connected\n";
            return;
        }

        on_order_update_ = [](const nlohmann::json& ord) {
            std::cout << "[ORDER UPDATE] "
                << ord.value("symbol", "") << " "
                << ord.value("orderStatus", "") << " "
                << ord.value("side", "") << "\n";
            };

        nlohmann::json sub;
        sub["op"] = "subscribe";
        sub["args"] = { "order" };
        auto ob = private_ws_->send(sub.dump());
        std::cout << "[bybit][privWS] subscribed: order\n";

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



         std::string api_key_ = std::string("yPUu6gX0AjQcqRmkR9");
         std::string  api_secret_ = std::string("uSgxbupka4la3siZ2T0buZoUbsHJBf3JHIMU");

        //Если нет ключа - выкидваем ошибку 
        if (api_key_.empty() || api_secret_.empty()) {
            throw ExchangeError("Bybit place_order: API credentials not set");
        };

        nlohmann::json j;
        // Нам нужно еще получать категорию
        j["category"] = "spot";
        j["symbol"] = spec.symbol;
        j["side"] = spec.side;


		// Тип ордера
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

		// Собираем тело запроса и делаем подготовку
        std::string body = j.dump();
        std::string method = "POST";
        std::string path = "/v5/order/create";
		// Bybit требует recvWindow для подписи м млсекунд
        std::string recvWindow = "5000";


        nlohmann::json sign_payload;

        sign_payload["body"] = body;
		sign_payload["api_key"] = api_key_;
		sign_payload["recvWindow"] = recvWindow;
		std::string payload_str = sign_payload.dump();
		std::cout << "Bybit place_order payload: " << payload_str << "\n";


        TP2::crypto::BybitSigner signer;        
        std::string sign = signer.sign(payload_str);
        

        TP2::net::HttpRequest req;

        req.method = method;
        req.url = "https://api-testnet.bybit.com/v5/spread/order/create";
        req.body = body;

        // Хедеры запроса
        req.headers["X-BAPI-SIGN"] = sign; // Обязательная подпись
        req.headers["X-BAPI-API-KEY"] = api_key_; // Ключ
		req.headers["X-BAPI-TIMESTAMP"] = TP2::crypto::now_ms_string(); // Текущее время в мс   
		req.headers["X-BAPI-RECV-WINDOW"] = recvWindow; // Окно получения (в миллесикундах)
        req.headers["Content-Type"] = "application/json";


		std::cout << "API KEY: " << api_key_ << "\n"
			<< "X-BAPI-TIMESTAMP: " << req.headers["X-BAPI-TIMESTAMP"] << "\n"
			<< "X-BAPI-SIGN: " << sign << "\n"
            ;
        
        // Отправка запроса
        auto res = http_->send(req);

		std::cout << "Bybit place_order response: " << res.body << "\n";    

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
            //ExchangeError err("Bybit error: " + response.dump());
            //err.vendor_code = response.value("retCode", -1);
            //err.retryable = false;
            //throw err;

			std::cerr << "Bybit place_order error: " << response.dump() << "\n";

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
        std::string recvWindow = "5000";



        nlohmann::json sign_payload;

        sign_payload["body"] = body;
        sign_payload["api_key"] = api_key_;
        sign_payload["rectWindow"] = recvWindow;

        std::string payload_str = sign_payload.dump();

        TP2::crypto::BybitSigner signer;

        std::string ts = TP2::crypto::now_ms_string();
        std::string sign = signer.sign(payload_str);

        TP2::net::HttpRequest req;

        req.method = method;
        req.url = "https://api-testnet.bybit.com" + path;


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
		std::string recvWindow = "5000";


        nlohmann::json sign_payload;

        sign_payload["body"] = body;
        sign_payload["api_key"] = api_key_;
        sign_payload["rectWindow"] = recvWindow;

        std::string payload_str = sign_payload.dump();

        TP2::crypto::BybitSigner signer;

        // Создаем подпись
		std::string sign = signer.sign(payload_str);


        // Подготовка запроса
        TP2::net::HttpRequest req;
        req.method = method;
        req.url = "https://api.bybit.com" + path;
        req.body = body;

        // Добавляем хедеры
        req.headers["Content-Type"] = "application/json";
        req.headers["X-BAPI-API-KEY"] = api_key_;
        req.headers["X-BAPI-TIMESTAMP"] = ts;
        req.headers["X-BAPI-SIGN"] = sign;
        req.headers["X-BAPI-RECV-WINDOW"] = "5000";


		std::cout << "Body: " << body << "\n";

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


    void BybitAdapter::test_subscribe() {
        std::string key = "";
        std::string private_key = "";

        if (!private_ws_ready_) {
            connect_private_ws(key, private_key);
        };

        private_subscribe_position();
		private_subscribe_order();



    }


} // namespace TP2::ex

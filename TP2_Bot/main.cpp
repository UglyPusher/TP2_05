#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/binance_adapter.hpp"
#include "include/tp2_exchanges/bybit_adapter.hpp"
#include "include/tp2_exchanges/sim_adapter.hpp"

#include "include/tp2_net/http_winhttp.hpp"
#include "include/tp2_net/websocket_winhttp.hpp"

static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if(!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    std::string config = (argc > 1) ? argv[1] : "config.example.json";
    std::cout << "[bot] config=" << config << "\n";
    std::string cfg = slurp(config);
    if(cfg.empty()) std::cerr << "[bot] WARN: config not found or empty\n";

    auto http = std::make_shared<TP2::net::WinHttpClient>();
    http->set_timeout(15000, 15000, 30000);

    //auto http = std::make_shared<TP2::net::DummyHttpClient>();
    auto ws   = std::make_shared<TP2::net::WinWebSocketClient>();

    TP2::ex::BinanceAdapter binance(http, ws);
    TP2::ex::BybitAdapter   bybit(http, ws);
    TP2::ex::SimExchangeAdapter sim(http, ws);

    std::cout << "[bot] exchanges: " << binance.name() << ", " << bybit.name() << ", " << sim.name() << "\n";
    std::cout << "[bot] OK (skeleton).\n";
    
    // 1) Быстрый REST smoke: подтянем реальный снапшот Bybit и выведем топ-1
    {
        auto ob = bybit.get_orderbook("ETHUSDT", 5);
        if (ob.seq == 0 && ob.bids.empty() && ob.asks.empty()) {
            std::cerr << "[bybit][REST] FAIL: empty snapshot (network? rate limit?)\n";
        }
        else {
            std::cout << "[bybit][REST] seq=" << ob.seq << " ts=" << ob.ts
                << " bids=" << ob.bids.size() << " asks=" << ob.asks.size() << "\n";
            if (!ob.bids.empty()) std::cout << "  REST best bid: " << ob.bids[0].p << " / " << ob.bids[0].q << "\n";
            if (!ob.asks.empty()) std::cout << "  REST best ask: " << ob.asks[0].p << " / " << ob.asks[0].q << "\n";
        }
        
    }


    // Подписка и печать 1–2 апдейтов
    bybit.subscribe_orderbook("ETHUSDT", 5, [](const TP2::ex::OrderBook& ob) {
        std::cout << "[OB] seq=" << ob.seq << " ts=" << ob.ts
            << " bids=" << ob.bids.size() << " asks=" << ob.asks.size() << "\n";
        if (!ob.bids.empty()) std::cout << "  best bid: " << ob.bids[0].p << " / " << ob.bids[0].q << "\n";
        if (!ob.asks.empty()) std::cout << "  best ask: " << ob.asks[0].p << " / " << ob.asks[0].q << "\n";
        });

    // ждём 15 секунд стрима
    auto t_end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < t_end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // аккуратно закрываем сокет и выходим
    ws->close();
    std::cout << "[bybit][WS] streaming stopped (15s)\n";
    return 0;
}

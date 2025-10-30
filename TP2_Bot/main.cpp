#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_net/http_winhttp.hpp"
#include "include/tp2_net/websocket_winhttp.hpp"

//#include "include/tp2_exchanges/binance_adapter.hpp"
#include "include/tp2_exchanges/bybit_adapter.hpp"
//#include "include/tp2_exchanges/sim_adapter.hpp"


static std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if(!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    // CLI: --config=path.json  --net=winhttp|dummy
    std::string config = "config.example.json";
    std::string net = "winhttp";
    std::string symbol = "ETHUSDT";
    int depth = 5;
    
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--config=", 0) == 0) config = a.substr(9);
        else if (a.rfind("--net=", 0) == 0) net = a.substr(6);
        else if (i == 1 && a.size() && a[0] != '-') config = a; // совместимость со старым способом
        else if (a.rfind("--symbol=", 0) == 0) symbol = a.substr(9);
        else if (a.rfind("--depth=", 0) == 0) {
            try { depth = std::stoi(a.substr(8)); }
            catch (...) { depth = 5; }
        }
    }


    std::cout << "[bot] config=" << config << "\n";
    std::cout << "[bot] net=" << net << " symbol=" << symbol << " depth=" << depth << "\n"; 
    std::string cfg = slurp(config);
    if (cfg.empty()) {
        std::cerr << "[bot] ERROR: config not found or empty: " << config << "\n";
        //return 2;
    }

    std::shared_ptr<TP2::net::IHttpClient> http;
    std::shared_ptr<TP2::net::IWebSocket>  ws;
    if (net == "dummy") {
        http = std::make_shared<TP2::net::DummyHttpClient>();
        ws = std::make_shared<TP2::net::DummyWebSocket>();
        std::cout << "[bot] NET=dummy\n";
    }
    else if (net == "winhttp") {
        auto h = std::make_shared<TP2::net::WinHttpClient>();
        h->set_timeout(15000, 15000, 30000);
        http = std::move(h);
        ws = std::make_shared<TP2::net::WinWebSocketClient>();
        std::cout << "[bot] NET=winhttp\n";
    }
    else {
        std::cerr << "[bot] ERROR: unknown --net value: " << net << " (use winhttp|dummy)\n";
        return 2;
    }
    
    TP2::ex::BybitAdapter   bybit(http, ws);

    std::cout << "[bot] exchanges: " << bybit.name() << "\n";
    std::cout << "[bot] OK (skeleton).\n";
    
    // 1) Быстрый REST smoke: подтянем реальный снапшот Bybit и выведем топ-1
    {
        auto ob = bybit.get_orderbook(symbol, depth);
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

    // Подписка и печать апдейтов
    bybit.subscribe_orderbook(symbol, depth, [](const TP2::ex::OrderBook& ob) {
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
    ws->close(1000, "bot shutdown");
    std::cout << "[bybit][WS] streaming stopped (15s)\n";
    return 0;
}

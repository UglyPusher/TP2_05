#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include "include/tp2_net/http.hpp"
#include "include/tp2_net/websocket.hpp"
#include "include/tp2_exchanges/binance_adapter.hpp"
#include "include/tp2_exchanges/bybit_adapter.hpp"
#include "include/tp2_exchanges/sim_adapter.hpp"

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

    auto http = std::make_shared<TP2::net::DummyHttpClient>();
    auto ws   = std::make_shared<TP2::net::DummyWebSocket>();

    TP2::ex::BinanceAdapter binance(http, ws);
    TP2::ex::BybitAdapter   bybit(http, ws);
    TP2::ex::SimExchangeAdapter sim(http, ws);

    std::cout << "[bot] exchanges: " << binance.name() << ", " << bybit.name() << ", " << sim.name() << "\n";
    std::cout << "[bot] OK (skeleton).\n";


    // ѕодписка и печать 1Ц2 апдейтов
    bybit.subscribe_orderbook("BTCUSDT", 5, [](const TP2::ex::OrderBook& ob) {
        std::cout << "[OB] seq=" << ob.seq << " ts=" << ob.ts
            << " bids=" << ob.bids.size() << " asks=" << ob.asks.size() << "\n";
        if (!ob.bids.empty()) std::cout << "  best bid: " << ob.bids[0].p << " / " << ob.bids[0].q << "\n";
        if (!ob.asks.empty()) std::cout << "  best ask: " << ob.asks[0].p << " / " << ob.asks[0].q << "\n";
        });

    // —ымитируем WS-снапшот и дельту (DummyWebSocket отдает on_message(msg) при send(msg))
    const char* bybit_snapshot = R"({
  "topic":"orderbook.50.BTCUSDT","type":"snapshot","ts": 1730000000000,
  "data":{"s":"BTCUSDT","u": 1000,
    "b":[["65000.0","0.3"],["64999.5","0.2"]],
    "a":[["65010.0","0.4"],["65011.0","0.1"]]
  }
})";
    const char* bybit_delta = R"({
  "topic":"orderbook.50.BTCUSDT","type":"delta","ts": 1730000000100,
  "data":{"s":"BTCUSDT","u": 1001,
    "b":[["65000.0","0.5"]], "a":[["65010.0","0.0"]]
  }
})";

    ws->send(bybit_snapshot);
    ws->send(bybit_delta);


    return 0;
}

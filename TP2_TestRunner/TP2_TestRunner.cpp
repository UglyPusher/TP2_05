
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





int main()
{

    std::shared_ptr<TP2::net::IHttpClient> http;
    std::shared_ptr<TP2::net::IWebSocket>  ws;

    http = std::make_shared<TP2::net::DummyHttpClient>();
    ws = std::make_shared<TP2::net::DummyWebSocket>();

    TP2::ex::BybitAdapter bybit(http, ws);

    if (!bybit.connect_public_ws()) {
        std::cerr << "WS connect failed\n";
        return 1;
    }



    TP2::ex::OrderSpec spec;
    spec.symbol = "BTCUSDT";
    spec.side = TP2::ex::Side::Buy;
    spec.type = TP2::ex::OrdType::Market;
    spec.qty = 0.01;

    bybit.place_order(spec);


    std::cin.get();
    return 0;

}

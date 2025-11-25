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

#include "include/tp2_exchanges/bybit_adapter.hpp"





int main()
{

    std::shared_ptr<TP2::net::IHttpClient> http;
    std::shared_ptr<TP2::net::IWebSocket>  ws;
    std::shared_ptr<TP2::net::IWebSocket>  private_ws;

    http = std::make_shared<TP2::net::WinHttpClient>();
    ws = std::make_shared<TP2::net::WinWebSocketClient>();
    private_ws = std::make_shared<TP2::net::WinWebSocketClient>();

    TP2::ex::BybitAdapter bybit(http, ws, private_ws);

    bybit.test_subscribe();

    std::cin.get();
    return 0;

}

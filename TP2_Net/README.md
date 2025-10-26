# TP2_Net

Сетевые абстракции и заглушки.

## Содержимое

- `include/tp2_net/http.hpp` — `IHttpClient`, `HttpRequest/Response`, `DummyHttpClient`.
- `include/tp2_net/websocket.hpp` — `IWebSocket`, `DummyWebSocket`.
- `src/http_client_dummy.cpp`, `src/websocket_dummy.cpp` — реализации заглушек.
- PCH: `pch.h/.cpp` (если используется).

## Назначение

- Изолировать транспорт (HTTP/WS) от логики адаптеров бирж.
- Позволить запускать и тестировать пайплайн без сети через `Dummy*`.

## План апгрейда

- `WinHTTPHttpClient` (GET/POST, таймауты).
- `WinWebSocketClient` (подписки, reconnect, ping/pong).

## Пример

```cpp
auto http = std::make_shared<TP2::net::DummyHttpClient>();
auto ws   = std::make_shared<TP2::net::DummyWebSocket>();
auto resp = http->send({ "GET", "https://example.com", {}, {} });
ws->on_message([](std::string_view m){ /* ... */ });
ws->connect("wss://..."); ws->send("{...}");
```

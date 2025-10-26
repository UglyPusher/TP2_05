# TP2_Exchanges

Адаптеры бирж + общий сборщик стакана.

## Содержимое

- `include/tp2_exchanges/exchange.hpp` — `IExchange`, `OrderSpec`, `OrderBook`.
- `include/tp2_exchanges/orderbook_assembler.hpp` — сборщик стакана (snapshot+delta+seq-guard).
- `bybit_adapter.*`, `binance_adapter.*`, `sim_adapter.*`.

## Bybit (V5) — статус

- REST `/v5/market/orderbook` → `get_orderbook()` возвращает `OrderBook`.
- WS `orderbook.50.<SYMBOL>` → `subscribe_orderbook()` парсит `type: snapshot|delta`, `data: { u, b, a }`, `ts` и применяет через `OrderBookAssembler`.
- Транспорт — пока `Dummy*` (для локальных тестов).

### Тест без сети

```cpp
bybit.subscribe_orderbook("BTCUSDT", 5, [](const TP2::ex::OrderBook& ob){
  std::cout << ob.seq << " " << ob.bids.size() << " " << ob.asks.size() << "\n";
});

ws->send(R"({"type":"snapshot","ts":1730,"data":{"u":1000,"b":[["65000","0.3"]],"a":[["65010","0.4"]]}})");
ws->send(R"({"type":"delta","ts":1731,"data":{"u":1001,"b":[["65000","0.5"]],"a":[["65010","0"]]}})");
```

## Binance — статус

- Каркас подключен, будет добавлен «боевой» парсер и snapshot.

## Sim

- `SimExchangeAdapter` — для интеграции с `TP2_SimServer`.

## План апгрейда

- Авто‑ресинк при gap (повторный REST).
- Private WS + торговля (place/cancel).

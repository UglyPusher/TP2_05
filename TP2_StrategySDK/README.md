# TP2_StrategySDK

SDK для стратегий (C ABI).

## Заголовок

- `include/tp2_strategy/sdk.h` — `TP2_StrategyApi` и `strat_on_*` функции.

## Как написать стратегию (DLL)

Экспортируемые функции:
- `bool strat_on_init(const char* json_config, const TP2_StrategyApi* api)`
- `void strat_on_start()`
- `void strat_on_market_data(const char* symbol, const void* orderbook, int64_t ts_ms)`
- `void strat_on_order_update(const char* symbol, const char* order_id, const char* status_json)`
- `void strat_on_timer(int64_t now_ms)`
- `void strat_on_shutdown()`

## План

- Пример стратегии (MA crossover) + загрузчик в `TP2_Bot`.

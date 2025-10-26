# TP2_05 — Trading Bot Solution

Монорепозиторий Visual Studio (x64, C++20) для торгового бота с коннекторами Binance/Bybit, сим-сервером бэктеста и SDK для стратегий.

## Состав решений / проектов

- **TP2_Net** — абстракции HTTP/WebSocket + заглушки `DummyHttpClient`/`DummyWebSocket` (для локальных тестов).
- **TP2_Crypto** — подписи/HMAC и криптопримитивы (пока заглушка `DummySigner`).
- **TP2_Exchanges** — интерфейсы бирж и адаптеры (`BinanceAdapter`, `BybitAdapter`, `SimExchangeAdapter`) + `OrderBookAssembler` (сборка стакана: snapshot → deltas, защита по последовательности).
- **TP2_StrategySDK** — C-интерфейс плагинов стратегий (заголовок `include/tp2_strategy/sdk.h`) + статическая либка.
- **TP2_SimServer** — заглушка сервера бэктеста (плейсхолдер WS/REST).
- **TP2_Bot** — консольное приложение (точка входа), wiring модулей и загрузка конфигурации.

## Требования

- Visual Studio 2022 (v143), x64
- C++20
- (Опционально) CMake — не требуется для VS-решения
- Windows 10+ SDK

## Сборка

1. Открой `TP2_05.sln` в Visual Studio 2022.
2. Выбери `x64` + `Debug` (или `Release`).
3. `Build → Rebuild Solution`.
4. Артефакты: `x64/<Config>/*.exe` и `*.lib`.

## Быстрый старт

- Запусти **TP2_Bot**: в текущем виде он создаёт `DummyHttpClient/DummyWebSocket` и подключает адаптеры.
- Для теста **Bybit** стакана можно «скормить» DummyWebSocket тестовым JSON (см. README проекта `TP2_Exchanges`).

## Конфигурация

Файл-пример `config.example.json` (в оригинальном скелете). Рекомендуемый формат:

```jsonc
{
  "mode": "paper",
  "exchanges": {
    "binance": { "ws": "wss://stream.binance.com:9443/ws", "rest": "https://api.binance.com" },
    "bybit":   { "ws": "wss://stream.bybit.com/v5/public/spot", "rest": "https://api.bybit.com" }
  },
  "symbols": ["BTCUSDT","ETHUSDT"],
  "orderbook_depth": 50,
  "strategies": [
    { "dll": "ma_cross.dll", "instance": "ma_fast12_slow26", "config": {"fast":12,"slow":26,"symbol":"BTCUSDT"} }
  ]
}
```

## Дорожная карта

1) Bybit: REST snapshot `/v5/market/orderbook` + WS `orderbook.50.<SYMBOL>` (уже каркас + мини‑парсер).  
2) Binance: snapshot + deltas (`lastUpdateId`, `U/u` guard).  
3) Реальные клиенты HTTP/WS (WinHTTP/WinWebSocket) взамен Dummy*.  
4) Торговля (V5): `place/cancel`, приватный WS (exec/order), RiskManager, Paper.  
5) Бэктест‑сервер: исторический реплей, latency, синтетические потоки.

## Структура каталогов (рекомендуемая)

```
TP2_05/
  TP2_Net/
  TP2_Crypto/
  TP2_Exchanges/
  TP2_StrategySDK/
  TP2_SimServer/
  TP2_Bot/
```

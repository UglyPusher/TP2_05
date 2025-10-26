# TP2_Crypto

Криптопримитивы и подписи.

## Содержимое

- `include/tp2_crypto/signer.hpp` — интерфейс `ISigner`, `DummySigner`.
- `src/signer_dummy.cpp` — фальш‑подпись для тестов.

## План

- HMAC‑SHA256 для Bybit/Binance (REST и private WS).
- Обёртка над Windows CNG или встроенная реализация.

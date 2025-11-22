#pragma once
#include "pch.h"
#include <cstdint>
#include <vector>
#include <string>
#include <span>

namespace tp2::exchanges {

// Уровень стакана
struct ObLevel {
    double price{0.0};
    double qty{0.0};
};

// ВНИМАНИЕ: разделяем стороны стакана и сделки, чтобы не промахнуться «в другую сторону»
enum class BookSide : uint8_t { Bid = 0, Ask = 1 };   // стакан
enum class TradeSide : uint8_t { Buy = 0, Sell = 1 };   // сделки/ордера

// (опционально) временная совместимость: выпилить после рефакторинга
//[[deprecated("Use BookSide or TradeSide explicitly")]]
//using Side = TradeSide;

// Дельта стакана (минимально необходимая)
struct ObDelta {
    TradeSide   side{ TradeSide::Buy};
    double price{0.0};
    double qty{0.0};     // qty==0 трактуем как удаление уровня
};

// Снимок стакана (для ресинка)
struct ObSnapshot {
    std::vector<ObLevel> bids; // отсортированы по убыванию
    std::vector<ObLevel> asks; // отсортированы по возрастанию
    std::uint64_t seq{0};
    std::int64_t  ts_ms{0};
    int depth{0}; // фактическая глубина снимка
};

// Тиковая сделка
struct Trade {
    double price{ 0.0 };
    double qty{ 0.0 };
    TradeSide side;      // без дефолта
    std::int64_t ts_ms{ 0 };
    Trade() = delete;
    Trade(double p, double q, TradeSide s, std::int64_t t) : price(p), qty(q), side(s), ts_ms(t) {}
};

// Утилиты
inline constexpr const char* to_string(BookSide s) { return s == BookSide::Bid ? "bid" : "ask"; }
inline constexpr const char* to_string(TradeSide s) { return s == TradeSide::Buy ? "buy" : "sell"; }
inline constexpr TradeSide opposite(TradeSide s) { return s == TradeSide::Buy ? TradeSide::Sell : TradeSide::Buy; }

} // namespace tp2::exchanges

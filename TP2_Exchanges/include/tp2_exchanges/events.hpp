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

enum class Side : uint8_t { Bid, Ask, Buy = Bid, Sell = Ask };

// Дельта стакана (минимально необходимая)
struct ObDelta {
    Side   side{Side::Bid};
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
    double price{0.0};
    double qty{0.0};
    Side   side{Side::Buy}; // агрессор (buy/sell)
    std::int64_t ts_ms{0};
};

} // namespace tp2::exchanges

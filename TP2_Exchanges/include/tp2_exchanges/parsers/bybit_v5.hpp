#pragma once
#include <string_view>
#include <vector>
#include <cstdint>

namespace TP2::ex::parse {

    struct PriceLevel { double p{}, q{}; };

    struct Book {
        bool        is_snapshot{ false }; // "snapshot" vs "delta"
        uint64_t    seq{ 0 };             // "u"
        long long   ts{ 0 };              // "ts"
        std::vector<PriceLevel> bids;   // "b": [[p,q],...]
        std::vector<PriceLevel> asks;   // "a": [[p,q],...]
    };

    // WS payload: {"type":"snapshot|delta","ts":...,"data":{"u":...,"b":[...],"a":[...]}}
    bool bybit_ws(std::string_view json, Book& out, int limit_levels);

    // REST snapshot: {"retCode":0,"result":{"u":...,"ts":...,"b":[...],"a":[...]}}
    bool bybit_rest(std::string_view json, Book& out, int limit_levels);

} // namespace TP2::ex::parse

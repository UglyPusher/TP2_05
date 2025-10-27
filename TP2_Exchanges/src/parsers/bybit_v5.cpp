#include "pch.h"
#include "include/tp2_exchanges/parsers/bybit_v5.hpp"
#include "nlohmann/json.hpp"
#include <string>

namespace TP2::ex::parse {

    static inline double to_double(const nlohmann::json& v) {
        if (v.is_number_float())   return v.get<double>();
        if (v.is_number_integer()) return static_cast<double>(v.get<long long>());
        if (v.is_string()) {
            const auto& s = v.get_ref<const std::string&>();
            try { return std::stod(s); }
            catch (...) { return 0.0; }
        }
        return 0.0;
    }

    static inline void parse_side(const nlohmann::json& arr, std::vector<PriceLevel>& out, int limit) {
        out.clear();
        if (!arr.is_array()) return;
        out.reserve(static_cast<size_t>(limit));
        int cnt = 0;
        for (const auto& lvl : arr) {
            if (cnt >= limit) break;
            if (!lvl.is_array() || lvl.size() < 2) continue;
            out.push_back({ to_double(lvl[0]), to_double(lvl[1]) });
            ++cnt;
        }
    }

    bool bybit_ws(std::string_view s, Book& out, int limit_levels) {
        out = {};
        nlohmann::json j;
        try { j = nlohmann::json::parse(s, nullptr, /*allow_exceptions*/true, /*ignore_comments*/true); }
        catch (...) { return false; }

        if (!j.contains("type") || !j["type"].is_string()) return false;
        out.is_snapshot = (j["type"] == "snapshot");

        if (j.contains("ts")) out.ts = static_cast<long long>(to_double(j["ts"]));

        if (j.contains("data") && j["data"].is_object()) {
            const auto& d = j["data"];
            if (d.contains("u")) out.seq = static_cast<uint64_t>(to_double(d["u"]));
            if (d.contains("b")) parse_side(d["b"], out.bids, limit_levels);
            if (d.contains("a")) parse_side(d["a"], out.asks, limit_levels);
        }
        return true;
    }

    bool bybit_rest(std::string_view s, Book& out, int limit_levels) {
        out = {};
        nlohmann::json j;
        try { j = nlohmann::json::parse(s, nullptr, /*allow_exceptions*/true, /*ignore_comments*/true); }
        catch (...) { return false; }

        if (!j.contains("retCode") || j["retCode"].get<int>() != 0) return false;
        if (!j.contains("result") || !j["result"].is_object()) return false;

        const auto& r = j["result"];
        if (r.contains("u"))  out.seq = static_cast<uint64_t>(to_double(r["u"]));
        if (r.contains("ts")) out.ts = static_cast<long long>(to_double(r["ts"]));
        if (r.contains("b"))  parse_side(r["b"], out.bids, limit_levels);
        if (r.contains("a"))  parse_side(r["a"], out.asks, limit_levels);

        out.is_snapshot = true;
        return true;
    }

} // namespace TP2::ex::parse

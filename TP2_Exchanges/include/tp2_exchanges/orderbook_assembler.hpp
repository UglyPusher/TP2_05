#pragma once
#include "include/tp2_exchanges/exchange.hpp"
#include <vector>
#include <algorithm>
#include <cstdint>

namespace TP2::ex {

    // Универсальный сборщик: snapshot -> deltas с seq-guard.
    class OrderBookAssembler {
    public:
        struct Delta {
            bool     is_bid{};  // true = bid, false = ask
            uint64_t seq{};     // абсолютная последовательность (или lastUpdateId для Binance)
            int64_t  ts{};      // timestamp события (если есть)
            double   p{};       // price
            double   q{};       // qty (0 => удалить уровень)
        };

        // Настройка жёсткости контроля последовательности:
        // strict=true → требуем d.seq == last_seq+1 (если обе стороны дают seq)
        // strict=false → допускаем d.seq > last_seq (перепрыгнули), просто примем и запомним
        void set_strict(bool v) { strict_seq_ = v; }
        bool strict() const { return strict_seq_; }

        void set_depth(int depth) { depth_ = std::max(1, depth); }
        int  depth() const { return depth_; }

        // Инициализация с полного снапшота
        void reset_from_snapshot(uint64_t seq, int64_t ts,
            std::vector<OrderBookLevel> bids,
            std::vector<OrderBookLevel> asks)
        {
            seq_ = seq; ts_ = ts;
            bids_ = std::move(bids);
            asks_ = std::move(asks);
            trim();
            healthy_ = true;
            expected_seq_ = (seq_ ? seq_ : 0);
        }

        // Применение одной дельты. false => обнаружен gap.
        [[nodiscard]] bool apply(const Delta& d) {
            if (!healthy_) return false;

            if (seq_ != 0 && d.seq != 0) {
                if (d.seq <= seq_) return true; // старая дельта — игнор
                if (strict_seq_) {
                    // требуем точно следующий id
                    const uint64_t exp = (expected_seq_ ? expected_seq_ + 1 : seq_ + 1);
                    if (d.seq != exp) { healthy_ = false; return false; } // gap
                }
                seq_ = d.seq;
                expected_seq_ = d.seq;
            }
            else if (d.seq != 0) {
                // если раньше seq_ был 0 (неизвестен), а теперь пришёл — инициализируем
                seq_ = d.seq;
                expected_seq_ = d.seq;
            }
            if (d.ts) ts_ = std::max<int64_t>(ts_, d.ts);

            auto& side = d.is_bid ? bids_ : asks_;
            upsert_level(side, d.p, d.q, d.is_bid);
            trim();
            return true;
        }

        bool healthy() const { return healthy_; }
        void mark_unhealthy() { healthy_ = false; }

        OrderBook snapshot() const {
            OrderBook ob;
            ob.bids = bids_;
            ob.asks = asks_;
            ob.seq = seq_;
            ob.ts = ts_;
            return ob;
        }

    private:
        static void upsert_level(std::vector<OrderBookLevel>& side, double p, double q, bool is_bid) {
            auto cmp = [is_bid](const OrderBookLevel& a, const OrderBookLevel& b) {
                return is_bid ? a.p > b.p : a.p < b.p;
                };
            // найти уровень по цене
            auto it = std::find_if(side.begin(), side.end(), [&](const OrderBookLevel& l) { return l.p == p; });
            if (q == 0.0) {
                if (it != side.end()) side.erase(it);
            }
            else {
                if (it != side.end()) it->q = q;
                else side.push_back({ p,q });
                std::sort(side.begin(), side.end(), cmp);
            }
        }

        void trim() {
            if ((int)bids_.size() > depth_) bids_.resize(depth_);
            if ((int)asks_.size() > depth_) asks_.resize(depth_);
        }

        int depth_{ 50 };
        bool strict_seq_{ true };
        uint64_t seq_{ 0 };
        uint64_t expected_seq_{ 0 };
        int64_t  ts_{ 0 };
        bool healthy_{ false };
        std::vector<OrderBookLevel> bids_, asks_;
    };

} // namespace TP2::ex

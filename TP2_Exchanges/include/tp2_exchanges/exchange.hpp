#pragma once
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <functional>

namespace TP2::ex {

enum class Side { Buy, Sell };
enum class OrdType { Market, Limit, Stop, StopLimit, PostOnly };

struct OrderSpec {
    std::string symbol; Side side; OrdType type;
    std::optional<double> price, qty, quote_qty, stop_price;
    std::optional<std::string> tif, client_id;
};

struct OrderBookLevel { double p, q; };
struct OrderBook { std::vector<OrderBookLevel> bids, asks; uint64_t seq{}; long long ts{}; };

struct OrderId { std::string exchange, native_id, client_id; };

struct ExchangeError : std::runtime_error {
    int http_status{}; int vendor_code{}; bool retryable{};
    using std::runtime_error::runtime_error;
};

struct IExchange {
    virtual ~IExchange() = default;
    virtual std::string name() const = 0;

    virtual OrderBook get_orderbook(std::string_view symbol, int depth) = 0;
    virtual void subscribe_orderbook(std::string_view symbol, int depth,
        std::function<void(const OrderBook&)>) = 0;

    virtual OrderId place_order(const OrderSpec&) = 0;
    virtual void cancel_order(std::string_view symbol, std::string_view exch_order_id) = 0;
    virtual void cancel_all_orders(std::string_view symbol) = 0;
};



// Проверка работоспособности
// Потом перепишу лучше
struct SocketPositionData {
    int64_t position_idx;
    int64_t trade_mode;
    int64_t risk_id;
    std::string risk_limit_value;
    std::string symbol;
    std::string side;
    std::string size;
    std::string entry_price;
    std::string leverage;
    std::string position_value;
    std::string position_balance;
    std::string mark_price;
    std::string position_im;
    std::string position_im_by_mp;
    std::string position_mm;
    std::string position_mm_by_mp;
    std::string take_profit;
    std::string stop_loss;
    std::string trailing_stop;
    std::string unrealised_pnl;
    std::string cur_realised_pnl;
    std::string cum_realised_pnl;
    std::string session_avg_price;
    std::string created_time;
    std::string updated_time;
    std::string tpsl_mode;
    std::string liq_price;
    std::string bust_price;
    std::string category;
    std::string position_status;
    int64_t adl_rank_indicator;
    int64_t auto_add_margin;
    std::string leverage_sys_updated_time;
    std::string mmr_sys_updated_time;
    int64_t seq;
    bool is_reduce_only;
};

struct SocketPosition {
    std::string id;
    std::string topic;
    int64_t creation_time;
    std::vector<SocketPositionData> data;
};


} // namespace TP2::ex

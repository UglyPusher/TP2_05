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

} // namespace TP2::ex

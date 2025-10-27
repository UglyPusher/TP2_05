#pragma once
#include "exchange.hpp"
#include <memory>
namespace TP2 { namespace net { struct IHttpClient; class IWebSocket; } namespace crypto { class ISigner; } }

namespace TP2::ex {
class BybitAdapter final : public IExchange {
public:
    BybitAdapter(std::shared_ptr<TP2::net::IHttpClient> http,
                 std::shared_ptr<TP2::net::IWebSocket> ws);
    std::string name() const override { return "bybit"; }

    // Market data
    OrderBook get_orderbook(std::string_view symbol, int depth) override;
    void subscribe_orderbook(std::string_view symbol, int depth, std::function<void(const OrderBook&)>) override;
    
    // Trading (stub for now)
    OrderId place_order(const OrderSpec&) override;
    void cancel_order(std::string_view symbol, std::string_view exch_order_id) override;
private:
    std::shared_ptr<TP2::net::IHttpClient> http_;
    std::shared_ptr<TP2::net::IWebSocket> ws_;
};
} // namespace msg5::ex

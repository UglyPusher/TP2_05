#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* symbol;
    double price;
    double qty;
    double stop_price;
    const char* tif;
    const char* client_id;
    int side;   // 0 Buy, 1 Sell
    int type;   // 0 Mkt, 1 Lmt, 2 Stop, 3 StopLimit, 4 PostOnly
} StratOrderSpec;

typedef struct {
    void (*place_order)(const StratOrderSpec*);
    void (*cancel_order)(const char* symbol, const char* order_id);
    void (*log)(int level, const char* msg);
} StrategyApi;

bool strat_on_init(const char* json_config, const StrategyApi* api);
void strat_on_start();
void strat_on_market_data(const char* symbol, const void* orderbook, int64_t ts_ms);
void strat_on_order_update(const char* symbol, const char* order_id, const char* status_json);
void strat_on_timer(int64_t now_ms);
void strat_on_shutdown();

#ifdef __cplusplus
}
#endif

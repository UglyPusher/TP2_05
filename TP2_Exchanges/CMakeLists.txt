add_library(MSG5_Exchanges STATIC
  src/exchange_iface.cpp
  src/binance_adapter.cpp
  src/bybit_adapter.cpp
  src/sim_adapter.cpp
  include/msg5_exchanges/exchange.hpp
  include/msg5_exchanges/binance_adapter.hpp
  include/msg5_exchanges/bybit_adapter.hpp
  include/msg5_exchanges/sim_adapter.hpp
)
target_include_directories(MSG5_Exchanges PUBLIC include)
target_link_libraries(MSG5_Exchanges PUBLIC MSG5_Net MSG5_Crypto)

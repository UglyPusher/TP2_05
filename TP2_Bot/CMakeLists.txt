add_executable(TP2_Bot
  main.cpp
)
target_include_directories(TP2_Bot PRIVATE include)
target_link_libraries(TP2_Bot PRIVATE TP2_Exchanges TP2_Net TP2_Crypto TP2_StrategySDK)

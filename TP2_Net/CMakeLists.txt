add_library(MSG5_Net STATIC
  src/http_client_dummy.cpp
  src/websocket_dummy.cpp
  include/msg5_net/http.hpp
  include/msg5_net/websocket.hpp
)
target_include_directories(MSG5_Net PUBLIC include)

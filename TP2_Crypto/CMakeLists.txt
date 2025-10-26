add_library(MSG5_Crypto STATIC
  src/signer_dummy.cpp
  include/msg5_crypto/signer.hpp
)
target_include_directories(MSG5_Crypto PUBLIC include)

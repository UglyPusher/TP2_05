#include "pch.h"
#include "include/tp2_crypto/signer.hpp"
#include <string>

namespace TP2::crypto {
std::string DummySigner::sign(std::string_view payload) {
    // not real HMAC — placeholder
    return std::string("sig(") + std::string(payload) + ")";
}
} // namespace msg5::crypto

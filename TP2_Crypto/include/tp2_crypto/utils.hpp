#pragma once
#include <string>

namespace TP2::crypto {

    std::string now_ms_string();
    std::string hex_encode(const unsigned char* d, size_t len);
    std::string hmac_sha256_hex(const std::string& key, const std::string& msg);
    std::string bybit_sign(
        const std::string& secret,
        const std::string& ts,
        const std::string& method,
        const std::string& path,
        const std::string& body
    );

} // namespace TP2::crypto

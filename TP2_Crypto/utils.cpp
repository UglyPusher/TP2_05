#pragma once
#include "pch.h"


#include <chrono>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string>


#include <openssl/core_names.h>
#include <openssl/params.h>
#include <vector>
#include <stdexcept>

namespace TP2::crypto {


    uint64_t now_ms() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
    }

    std::string now_ms_string() {
        using namespace std::chrono;
        auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
        return std::to_string(now.count());
        //using namespace std::chrono;
        //return std::to_string(
        //    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
        //);
    }

    std::string hex_encode(const unsigned char* data, size_t len) {
        static const char hex_chars[] = "0123456789abcdef";
        std::string result;
        result.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            result.push_back(hex_chars[(data[i] >> 4) & 0xF]);
            result.push_back(hex_chars[data[i] & 0xF]);
        }
        return result;
    }

    std::string hmac_sha256_hex(const std::string& key, const std::string& msg) {
        // Получаем алгоритм HMAC
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
        if (!mac) throw std::runtime_error("Failed to fetch HMAC");

        EVP_MAC_CTX* ctx = EVP_MAC_CTX_new(mac);
        EVP_MAC_free(mac);
        if (!ctx) throw std::runtime_error("Failed to create EVP_MAC_CTX");

        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, const_cast<char*>("SHA256"), 0),
            OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, const_cast<char*>(key.data()), key.size()),
            OSSL_PARAM_construct_end()
        };

        if (EVP_MAC_init(ctx, nullptr, 0, params) != 1) {
            EVP_MAC_CTX_free(ctx);
            throw std::runtime_error("EVP_MAC_init failed");
        }

        if (EVP_MAC_update(ctx, reinterpret_cast<const unsigned char*>(msg.data()), msg.size()) != 1) {
            EVP_MAC_CTX_free(ctx);
            throw std::runtime_error("EVP_MAC_update failed");
        }

        // HMAC-SHA256 всегда 32 байта
        unsigned char buf[32];
        size_t out_len = sizeof(buf);

        if (EVP_MAC_final(ctx, buf, &out_len, sizeof(buf)) != 1) {
            EVP_MAC_CTX_free(ctx);
            throw std::runtime_error("EVP_MAC_final failed");
        }

        EVP_MAC_CTX_free(ctx);

        return hex_encode(buf, out_len);
    }
    
    /*
    std::string hex_encode(const unsigned char* d, size_t len) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i)
            oss << std::setw(2) << (int)d[i];
        return oss.str();
    }

    std::string hmac_sha256_hex(const std::string& key, const std::string& msg) {
        unsigned char out[EVP_MAX_MD_SIZE];
        unsigned int out_len = 0;

        HMAC_CTX* ctx = HMAC_CTX_new();
        HMAC_Init_ex(ctx, key.data(), key.size(), EVP_sha256(), nullptr);
        HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(msg.data()), msg.size());
        HMAC_Final(ctx, out, &out_len);
        HMAC_CTX_free(ctx);

        return hex_encode(out, out_len);
    }

    */

    std::string bybit_sign(
        const std::string& api_key,
        const std::string& secret,
        const std::string& recvWindow,
        const std::string& ts,
        const std::string& body
    ) {
		// 5000 это recvWindow по умолчанию
        return hmac_sha256_hex(secret, ts + api_key + recvWindow + body);
    }


} // namespace TP2::crypto

#include <string>
#include <chrono>
#include <hex>
#include <oss>


namespace Crypto::tp {


        static std::string now_ms_string() {
            using namespace std::chrono;
            return std::to_string(
                duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()
            );
        }

        static std::string hex_encode(const unsigned char* d, size_t len) {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (size_t i = 0; i < len; ++i)
                oss << std::setw(2) << (int)d[i];
            return oss.str();
        }

        static std::string hmac_sha256_hex(const std::string& key, const std::string& msg) {
            unsigned char out[EVP_MAX_MD_SIZE];
            unsigned int out_len = 0;

            HMAC_CTX* ctx = HMAC_CTX_new();
            HMAC_Init_ex(ctx, key.data(), key.size(), EVP_sha256(), nullptr);
            HMAC_Update(ctx, (const unsigned char*)msg.data(), msg.size());
            HMAC_Final(ctx, out, &out_len);
            HMAC_CTX_free(ctx);

            return hex_encode(out, out_len);
        }

        // Bybit v5 signature = SHA256(secret, timestamp + method + path + body)
        static std::string bybit_sign(
            const std::string& secret,
            const std::string& ts,
            const std::string& method,
            const std::string& path,
            const std::string& body
        ) {
            return hmac_sha256_hex(secret, ts + method + path + body);
        }
}




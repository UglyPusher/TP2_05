#pragma once
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace TP2::crypto {


    struct ISigner {
        virtual ~ISigner() = default;
        virtual std::string sign(std::string_view payload) = 0;
    };


    class BybitSigner final : public ISigner {
    public:
        std::string sign(std::string_view payload) override;
    };

    class BinanceSigner final : public ISigner {
        std::string sign(std::string_view payload) override;
    };

    class DummySigner final : public ISigner {
    public:
        std::string sign(std::string_view payload) override;
    };

}; // namespace msg5::crypto

#pragma once
#include <string>

namespace TP2::crypto {

struct ISigner {
    virtual ~ISigner() = default;
    virtual std::string sign(std::string_view payload) = 0;
};

class DummySigner final : public ISigner {
public:
    std::string sign(std::string_view payload) override;
};

} // namespace msg5::crypto

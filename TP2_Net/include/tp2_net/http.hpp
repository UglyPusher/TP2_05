#pragma once
#include <string>
#include <map>
#include <string_view>

namespace TP2::net {

    struct HttpRequest {
        std::string method, url, body;
        std::map<std::string, std::string> headers;
    };

    struct HttpResponse {
        int status{};
        std::string body;
        std::map<std::string, std::string> headers;
    };

    // БАЗА: виртуальный деструктор без noexcept (и без =0), out-of-line определим в .cpp
    struct IHttpClient {
        virtual ~IHttpClient();
        virtual HttpResponse send(const HttpRequest& req) = 0;
    };

    // НАСЛЕДНИК: деструктор объявлен точно той же сигнатурой + override
    class DummyHttpClient final : public IHttpClient {
    public:
        ~DummyHttpClient() override;                 // определим в .cpp
        HttpResponse send(const HttpRequest& req) override;
    };

} // namespace TP2::net

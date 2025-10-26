#include "pch.h"
#include "include/tp2_net/http.hpp"
#include <string>

namespace TP2::net {

    // out-of-line определения деструкторов — убирает “implicitly deleted” у MSVC
    IHttpClient::~IHttpClient() = default;
    DummyHttpClient::~DummyHttpClient() = default;

    HttpResponse DummyHttpClient::send(const HttpRequest& req) {
        HttpResponse r;
        r.status = 200;
        r.body = std::string("{\"ok\":true,\"echo_url\":\"") + req.url + "\"}";
        return r;
    }

} // namespace TP2::net

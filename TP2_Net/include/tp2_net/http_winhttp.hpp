#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#include "include/tp2_net/http.hpp"  // TP2::net::HttpRequest/HttpResponse/IHttpClient

namespace TP2::net {

class WinHttpClient final : public IHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient() override;

    void set_timeout(int connect_ms, int send_ms, int recv_ms);
    HttpResponse send(const HttpRequest& req) override;

private:
    HINTERNET hSession_{nullptr};
    int t_connect_{15015};
    int t_send_{15015};
    int t_recv_{30030};

    struct UrlParts {
        std::wstring host;
        std::wstring path_query;
        INTERNET_PORT port{0};
        bool secure{false};
    };

    static std::wstring to_wide(const std::string& s);
    static UrlParts crack_url(const std::string& url);

    HttpResponse do_request(const std::wstring& method_w,
                            const UrlParts& u,
                            const std::string& body,
                            const std::wstring& extra_headers_w);
};

} // namespace TP2::net

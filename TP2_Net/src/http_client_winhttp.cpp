#include "pch.h"
#include "include/tp2_net/http_winhttp.hpp"
#include <stdexcept>
#include <sstream>

namespace TP2::net {

WinHttpClient::WinHttpClient() {
    hSession_ = WinHttpOpen(L"TP2/WinHttpClient",
                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (hSession_) {
        WinHttpSetTimeouts(hSession_, t_connect_, t_send_, t_recv_, t_recv_);
    }
}

WinHttpClient::~WinHttpClient() {
    if (hSession_) {
        WinHttpCloseHandle(hSession_);
        hSession_ = nullptr;
    }
}

void WinHttpClient::set_timeout(int connect_ms, int send_ms, int recv_ms) {
    if (connect_ms > 0) t_connect_ = connect_ms;
    if (send_ms > 0)    t_send_    = send_ms;
    if (recv_ms > 0)    t_recv_    = recv_ms;
    if (hSession_) WinHttpSetTimeouts(hSession_, t_connect_, t_send_, t_recv_, t_recv_);
}

std::wstring WinHttpClient::to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

WinHttpClient::UrlParts WinHttpClient::crack_url(const std::string& url) {
    UrlParts out;
    // Ќормализуем минимально: '\' -> '/', и гарантируем 'scheme://'
    std::string url2 = url;
    if (url2.rfind("wss:", 0) == 0)      url2.replace(0, 4, "https:");
    else if (url2.rfind("ws:", 0) == 0)  url2.replace(0, 3, "http:");
    for (auto& ch : url2) if (ch == '\\') ch = '/';
    const size_t colon = url2.find(':');
    if (colon != std::string::npos) {
        size_t i = colon + 1, cnt = 0;
        while (i < url2.size() && url2[i] == '/') { ++cnt; ++i; }
        if (cnt < 2) url2.insert(colon + 1, std::string(2 - cnt, '/'));
    }
    auto wurl = to_wide(url2);


    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t scheme[16]{}; uc.lpszScheme = scheme; uc.dwSchemeLength = (DWORD)std::size(scheme);
    wchar_t host[256]{};  uc.lpszHostName = host; uc.dwHostNameLength = (DWORD)std::size(host);
    wchar_t path[2048]{}; uc.lpszUrlPath = path; uc.dwUrlPathLength = (DWORD)std::size(path);
    wchar_t extra[2048]{};uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = (DWORD)std::size(extra);

    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) {
        return out;
    }
    out.port = uc.nPort;
    out.secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);

    std::wstring p;
    if (uc.dwUrlPathLength) p.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) p.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (p.empty()) p = L"/";
    out.path_query = std::move(p);
    return out;
}

HttpResponse WinHttpClient::do_request(const std::wstring& method_w,
                                       const UrlParts& u,
                                       const std::string& body,
                                       const std::wstring& extra_headers_w) {
    if (!hSession_ || u.port == 0 || u.host.empty() || u.path_query.empty())
        return {0, ""};

    HINTERNET hConnect = WinHttpConnect(hSession_, u.host.c_str(), u.port, 0);
    if (!hConnect) return {0, ""};

    DWORD flags = u.secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method_w.c_str(),
                                            u.path_query.c_str(), NULL,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); return {0, ""}; }

    BOOL ok = WinHttpSendRequest(hRequest,
                                 extra_headers_w.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extra_headers_w.c_str(),
                                 extra_headers_w.empty() ? 0 : (DWORD)-1L,
                                 body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
                                 (DWORD)body.size(),
                                 (DWORD)body.size(),
                                 0);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); return {0, ""}; }

    ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); return {0, ""}; }

    DWORD status=0, len=sizeof(status);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &len, WINHTTP_NO_HEADER_INDEX);

    std::string resp;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0) break;
        std::string chunk; chunk.resize(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), avail, &read) || read == 0) break;
        resp.append(chunk.data(), read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return {(int)status, std::move(resp)};
}

HttpResponse WinHttpClient::send(const HttpRequest& req) {
    auto parts = crack_url(req.url);
    if (parts.port == 0) return {0, ""};
    std::wstring method_w = to_wide(req.method.empty() ? std::string("GET") : req.method);
    std::wstring headers_w; // not used for now
    return do_request(method_w, parts, req.body, headers_w);
}

} // namespace TP2::net

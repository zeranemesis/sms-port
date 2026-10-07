#include "retroachievements_http.h"
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif
namespace sms_frontend { namespace ra_http {

namespace {
constexpr long kTimeoutSeconds = 30;
}

// Each backend implements this once; post() and get() at the end of the file
// are the public entry points. A GET passes an empty body and content type.
static Response perform(const char* method, const std::string& url, const std::string& body,
    const std::string& contentType, const std::string& userAgent);

#ifdef _WIN32

namespace {

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string last_error(const char* what) {
    return std::string(what) + " failed (error " + std::to_string(GetLastError()) + ")";
}

// Closes a WinHTTP handle when it goes out of scope.
struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h != nullptr) {
            WinHttpCloseHandle(h);
        }
    }
};

} // namespace

bool available() { return true; }

static Response perform(const char* method, const std::string& url, const std::string& body,
    const std::string& contentType, const std::string& userAgent) {
    Response response;
    const std::wstring wideUrl = widen(url);

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) {
        response.error = last_error("WinHttpCrackUrl");
        return response;
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo != nullptr) {
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }

    Handle session{WinHttpOpen(widen(userAgent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0)};
    if (session.h == nullptr) {
        response.error = last_error("WinHttpOpen");
        return response;
    }
    const int timeoutMs = static_cast<int>(kTimeoutSeconds * 1000);
    WinHttpSetTimeouts(session.h, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    Handle connection{WinHttpConnect(session.h, host.c_str(), parts.nPort, 0)};
    if (connection.h == nullptr) {
        response.error = last_error("WinHttpConnect");
        return response;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS || (host != L"retroachievements.org" && host != L"www.retroachievements.org")) { response.error = "Unsupported HTTPS service"; return response; }
    const DWORD flags = WINHTTP_FLAG_SECURE;
    Handle request{WinHttpOpenRequest(connection.h, widen(method).c_str(), path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
    if (request.h == nullptr) {
        response.error = last_error("WinHttpOpenRequest");
        return response;
    }

    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    const std::wstring headers =
        contentType.empty() ? std::wstring() : L"Content-Type: " + widen(contentType) + L"\r\n";
    if (!WinHttpSendRequest(request.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
            headers.empty() ? 0 : static_cast<DWORD>(-1), const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0)
        || !WinHttpReceiveResponse(request.h, nullptr)) {
        response.error = last_error("WinHttpSendRequest");
        return response;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        response.error = last_error("WinHttpQueryHeaders");
        return response;
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.h, &available)) {
            response.error = last_error("WinHttpQueryDataAvailable");
            return response;
        }
        if (available == 0) {
            break;
        }
        if (response.body.size() + available > 16 * 1024 * 1024) { response.error = "Response too large"; response.body.clear(); return response; }
        const size_t offset = response.body.size();
        response.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.h, response.body.data() + offset, available, &read)) {
            response.error = last_error("WinHttpReadData");
            response.body.clear();
            return response;
        }
        response.body.resize(offset + read);
    }
    response.status = static_cast<int>(status);
    return response;
}


#else
bool available() { return false; }
static Response perform(const char*,const std::string&,const std::string&,const std::string&,const std::string&) { return {}; }
#endif
Response request(const std::string& url,const std::string& body,const std::string& type,const std::string& agent) { return perform(body.empty()?"GET":"POST",url,body,type,agent); }
} }

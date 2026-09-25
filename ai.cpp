#include "ai.h"
#include "config.h"
#include "logger.h"
#include <winhttp.h>
#include <gdiplus.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")

// ========== Screenshot as base64 (with downscale) ==========
std::string CaptureScreenBase64() {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    SelectObject(hMem, hBmp);
    BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);

    Gdiplus::Bitmap bitmap(hBmp, nullptr);

    // Downscale to max width 1280 to save tokens
    const int MAX_W = 1280;
    Gdiplus::Bitmap* pSave = &bitmap;
    Gdiplus::Bitmap* pScaled = nullptr;
    if (w > MAX_W) {
        int newW = MAX_W;
        int newH = (int)(h * (double)MAX_W / w);
        pScaled = new Gdiplus::Bitmap(newW, newH, PixelFormat32bppARGB);
        Gdiplus::Graphics g(pScaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(&bitmap, 0, 0, newW, newH);
        pSave = pScaled;
    }

    IStream* pStream = nullptr;
    CreateStreamOnHGlobal(nullptr, TRUE, &pStream);

    CLSID pngClsid;
    CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &pngClsid);
    pSave->Save(pStream, &pngClsid, nullptr);

    STATSTG stat;
    pStream->Stat(&stat, STATFLAG_NONAME);
    ULONG size = stat.cbSize.LowPart;

    HGLOBAL hGlobal;
    GetHGlobalFromStream(pStream, &hGlobal);
    void* pData = GlobalLock(hGlobal);

    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((size + 2) / 3) * 4);
    for (ULONG i = 0; i < size; i += 3) {
        int b = ((unsigned char*)pData)[i] << 16;
        if (i + 1 < size) b |= ((unsigned char*)pData)[i + 1] << 8;
        if (i + 2 < size) b |= ((unsigned char*)pData)[i + 2];
        result += b64[(b >> 18) & 63];
        result += b64[(b >> 12) & 63];
        result += (i + 1 < size) ? b64[(b >> 6) & 63] : '=';
        result += (i + 2 < size) ? b64[b & 63] : '=';
    }

    GlobalUnlock(hGlobal);
    pStream->Release();
    if (pScaled) delete pScaled;
    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(nullptr, hScreen);

    return result;
}

// ========== Local helpers ==========
static std::wstring U2WLocal(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static void ParseUrl(const std::string& url, std::wstring& host, std::wstring& path) {
    std::string u = url;
    size_t pos = u.find("://");
    if (pos != std::string::npos) u = u.substr(pos + 3);
    size_t slash = u.find('/');
    std::string h, p;
    if (slash == std::string::npos) { h = u; p = "/"; }
    else { h = u.substr(0, slash); p = u.substr(slash); }
    host = U2WLocal(h);
    path = U2WLocal(p);
}

// ========== Call AI ==========
std::string CallAIVision(const std::string& prompt, const std::string& imageBase64) {
    if (g_config.api_key.empty()) {
        return "ERROR: AUTH_FAILED";
    }
    if (g_config.base_url.empty()) {
        return "ERROR: WinHttpConnect failed";
    }

    std::wstring host, path;
    ParseUrl(g_config.base_url, host, path);

    json body;
    try {
        json content = json::array();
        content.push_back({ {"type", "text"}, {"text", CleanUTF8(prompt)} });
        content.push_back({
            {"type", "image_url"},
            {"image_url", {{"url", "data:image/png;base64," + imageBase64}}}
            });
        body = {
            {"model", g_config.model},
            {"messages", {{{"role", "user"}, {"content", content}}}},
            {"temperature", 0}
        };
    }
    catch (...) { return "ERROR: build request failed"; }

    std::string body_str = body.dump();

    HINTERNET hSession = WinHttpOpen(L"AIAuto/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "ERROR: WinHttpOpen failed";

    // ★ Timeouts so the request can't hang forever
    WinHttpSetTimeouts(hSession,
        5000,    // resolve
        5000,    // connect
        10000,   // send
        30000    // receive (30s max)
    );

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpConnect failed";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpOpenRequest failed";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + U2WLocal(g_config.api_key) + L"\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body_str.c_str(), body_str.size(), body_str.size(), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpSendRequest failed";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpReceiveResponse failed";
    }

    // HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    std::string response;
    DWORD size = 0;
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;
        char* buffer = new char[size + 1];
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buffer, size, &read)) {
            delete[] buffer;
            break;
        }
        response.append(buffer, read);
        delete[] buffer;
    } while (size > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (statusCode == 401) return "ERROR: AUTH_FAILED";
    if (statusCode == 403) return "ERROR: PERMISSION_DENIED";
    if (statusCode == 429) return "ERROR: RATE_LIMITED";
    if (statusCode == 402) return "ERROR: INSUFFICIENT_QUOTA";
    if (statusCode == 400) return "ERROR: BAD_REQUEST";
    if (statusCode >= 500) return "ERROR: SERVER_ERROR";
    if (statusCode != 200) return "ERROR: HTTP_" + std::to_string(statusCode);

    if (response.empty()) return "ERROR: EMPTY_RESPONSE";

    json resp = SafeParseJSON(response);
    if (resp.is_discarded()) return "ERROR: INVALID_JSON";

    if (resp.contains("error")) {
        std::string errMsg;
        try {
            if (resp["error"].is_object()) {
                errMsg = resp["error"].value("message", "");
                std::string code = resp["error"].value("code", "");
                if (errMsg.find("quota") != std::string::npos ||
                    errMsg.find("balance") != std::string::npos ||
                    errMsg.find("insufficient") != std::string::npos ||
                    code.find("quota") != std::string::npos) {
                    return "ERROR: INSUFFICIENT_QUOTA";
                }
                if (errMsg.find("invalid_api_key") != std::string::npos ||
                    errMsg.find("Invalid API-key") != std::string::npos) {
                    return "ERROR: AUTH_FAILED";
                }
            }
            else if (resp["error"].is_string()) {
                errMsg = resp["error"].get<std::string>();
            }
        }
        catch (...) {}
        return "ERROR: API_ERROR: " + errMsg;
    }

    try {
        if (!resp.contains("choices") || resp["choices"].empty())
            return "ERROR: NO_CHOICES";
        return resp["choices"][0]["message"]["content"].get<std::string>();
    }
    catch (...) { return "ERROR: PARSE_FAILED"; }
}
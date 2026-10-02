#include "ai.h"
#include "config.h"
#include "logger.h"
#include <winhttp.h>
#include <gdiplus.h>
#include <iostream>
#include <new>
#include <atomic>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")

// ★ 从 main.cpp 暴露
extern bool IsStopRequested();

// ★ 当前正在进行的请求 handle，用于中断
static std::atomic<HINTERNET> g_currentRequest{ nullptr };

void AbortCurrentAIRequest() {
    HINTERNET h = g_currentRequest.exchange(nullptr);
    if (h) {
        std::cout << "[AI] AbortCurrentAIRequest: closing active handle" << std::endl;
        WinHttpCloseHandle(h);
    }
}

// ========== Screenshot ==========
static int g_screenshotMaxW = 960;

std::string CaptureScreenBase64() {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0) return "";

    HDC hScreen = GetDC(nullptr);
    if (!hScreen) return "";
    HDC hMem = CreateCompatibleDC(hScreen);
    if (!hMem) { ReleaseDC(nullptr, hScreen); return ""; }
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    if (!hBmp) { DeleteDC(hMem); ReleaseDC(nullptr, hScreen); return ""; }
    HGDIOBJ hOld = SelectObject(hMem, hBmp);
    BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);

    Gdiplus::Bitmap bitmap(hBmp, nullptr);

    const int MAX_W = g_screenshotMaxW;
    Gdiplus::Bitmap* pSave = &bitmap;
    Gdiplus::Bitmap* pScaled = nullptr;
    if (w > MAX_W) {
        int newW = MAX_W;
        int newH = (int)(h * (double)MAX_W / w);
        pScaled = new Gdiplus::Bitmap(newW, newH, PixelFormat32bppARGB);
        if (pScaled && pScaled->GetLastStatus() == Gdiplus::Ok) {
            Gdiplus::Graphics g(pScaled);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage(&bitmap, 0, 0, newW, newH);
            pSave = pScaled;
        }
        else {
            if (pScaled) { delete pScaled; pScaled = nullptr; }
            pSave = &bitmap;
        }
    }

    IStream* pStream = nullptr;
    HRESULT hrStream = CreateStreamOnHGlobal(nullptr, TRUE, &pStream);
    if (FAILED(hrStream) || !pStream) {
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }

    CLSID pngClsid;
    HRESULT hrClsid = CLSIDFromString(
        const_cast<LPOLESTR>(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}"), &pngClsid);
    if (FAILED(hrClsid)) {
        pStream->Release();
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }

    Gdiplus::Status saveStatus = pSave->Save(pStream, &pngClsid, nullptr);
    if (saveStatus != Gdiplus::Ok) {
        pStream->Release();
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }

    STATSTG stat;
    HRESULT hrStat = pStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hrStat) || stat.cbSize.LowPart == 0) {
        pStream->Release();
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }
    ULONG size = stat.cbSize.LowPart;

    HGLOBAL hGlobal = nullptr;
    HRESULT hrHg = GetHGlobalFromStream(pStream, &hGlobal);
    if (FAILED(hrHg) || !hGlobal) {
        pStream->Release();
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }

    void* pData = GlobalLock(hGlobal);
    if (!pData) {
        pStream->Release();
        if (pScaled) delete pScaled;
        SelectObject(hMem, hOld); DeleteObject(hBmp);
        DeleteDC(hMem); ReleaseDC(nullptr, hScreen);
        return "";
    }

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
    SelectObject(hMem, hOld); DeleteObject(hBmp);
    DeleteDC(hMem); ReleaseDC(nullptr, hScreen);

    std::cout << "[AI] Screenshot: " << w << "x" << h
        << " png=" << size << " base64=" << result.size() << std::endl;

    return result;
}

// ========== Helpers ==========
static std::wstring U2WLocal(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string W2ULocal(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
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
    if (g_config.api_key.empty()) return "ERROR: AUTH_FAILED";
    if (g_config.base_url.empty()) return "ERROR: WinHttpConnect failed";
    if (imageBase64.empty()) return "ERROR: NO_IMAGE";

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

    std::cout << "[AI] === Request ===" << std::endl;
    std::cout << "[AI] host=" << W2ULocal(host) << std::endl;
    std::cout << "[AI] model=" << g_config.model << std::endl;
    std::cout << "[AI] api_key_len=" << g_config.api_key.size() << std::endl;
    std::cout << "[AI] prompt_len=" << prompt.size()
        << " image_b64_len=" << imageBase64.size()
        << " body_len=" << body_str.size() << std::endl;

    HINTERNET hSession = WinHttpOpen(L"AIAuto/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "ERROR: WinHttpOpen failed";

    // ★ receive 超时 15s
    WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 15000);

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

    // ★ 注册当前请求，以便 AbortCurrentAIRequest 中断
    g_currentRequest.store(hRequest);

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + U2WLocal(g_config.api_key) + L"\r\n";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (IsStopRequested()) {
        g_currentRequest.store(nullptr);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: ABORTED";
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)body_str.c_str(), (DWORD)body_str.size(), (DWORD)body_str.size(), 0)) {
        g_currentRequest.store(nullptr);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpSendRequest failed";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        g_currentRequest.store(nullptr);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpReceiveResponse failed";
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    std::string response;
    DWORD size = 0;
    do {
        if (IsStopRequested()) {
            std::cout << "[AI] aborted during read" << std::endl;
            g_currentRequest.store(nullptr);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "ERROR: ABORTED";
        }
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;
        char* buffer = new (std::nothrow) char[size + 1];
        if (!buffer) break;
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buffer, size, &read)) {
            delete[] buffer;
            break;
        }
        response.append(buffer, read);
        delete[] buffer;
    } while (size > 0);

    g_currentRequest.store(nullptr);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    std::cout << "[AI] status=" << statusCode
        << " body_len=" << response.size() << std::endl;

    if (statusCode != 200) {
        std::string detail = response;
        if (detail.size() > 2000) detail = detail.substr(0, 2000) + "...(truncated)";
        std::cout << "[AI] HTTP " << statusCode << " body: " << detail << std::endl;
    }

    if (statusCode == 401) return "ERROR: AUTH_FAILED";
    if (statusCode == 403) return "ERROR: PERMISSION_DENIED";
    if (statusCode == 429) return "ERROR: RATE_LIMITED";
    if (statusCode == 402) return "ERROR: INSUFFICIENT_QUOTA";
    if (statusCode == 400) {
        std::string detail = response;
        if (detail.size() > 800) detail = detail.substr(0, 800) + "...(truncated)";
        return "ERROR: BAD_REQUEST: " + detail;
    }
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
        std::string content = resp["choices"][0]["message"]["content"].get<std::string>();
        std::cout << "[AI] reply=" << content << std::endl;
        return content;
    }
    catch (...) { return "ERROR: PARSE_FAILED"; }
}
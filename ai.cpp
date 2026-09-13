#include "ai.h"
#include <winhttp.h>
#include <gdiplus.h>
#include <iostream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")

// ========== 截图（base64）==========
std::string CaptureScreenBase64() {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    SelectObject(hMem, hBmp);
    BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);

    Gdiplus::Bitmap bitmap(hBmp, nullptr);

    IStream* pStream = nullptr;
    CreateStreamOnHGlobal(nullptr, TRUE, &pStream);

    CLSID pngClsid;
    CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &pngClsid);
    bitmap.Save(pStream, &pngClsid, nullptr);

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
    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(nullptr, hScreen);

    return result;
}

// ========== 调 AI 视觉模型 ==========
std::string CallAIVision(const std::string& prompt, const std::string& imageBase64) {
    json body;
    try {
        json content = json::array();
        content.push_back({ {"type", "text"}, {"text", CleanUTF8(prompt)} });
        content.push_back({
            {"type", "image_url"},
            {"image_url", {{"url", "data:image/png;base64," + imageBase64}}}
            });
        body = {
            {"model", MODEL_VISION},
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

    HINTERNET hConnect = WinHttpConnect(hSession, API_HOST.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpConnect failed";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", API_PATH.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "ERROR: WinHttpOpenRequest failed";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + U2W(API_KEY) + L"\r\n";
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

    if (response.empty()) return "ERROR: empty response";

    json resp = SafeParseJSON(response);
    if (resp.is_discarded()) return "ERROR: invalid JSON";

    try {
        if (!resp.contains("choices") || resp["choices"].empty())
            return "ERROR: bad response";
        return resp["choices"][0]["message"]["content"].get<std::string>();
    }
    catch (...) { return "ERROR: parse failed"; }
}
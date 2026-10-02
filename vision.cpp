#include "vision.h"
#include "ai.h"
#include "uia.h"
#include "input.h"
#include <iostream>

// ★ 从 main.cpp 暴露
extern bool IsStopRequested();

// ============ 截图缓存（2 秒内复用） ============
static std::string g_cachedScreenshot;
static DWORD       g_cachedScreenshotTime = 0;

static std::string GetCachedScreenshot() {
    DWORD now = GetTickCount();
    if (!g_cachedScreenshot.empty() && (now - g_cachedScreenshotTime) < 2000) {
        std::cout << "[VISION] reuse cached screenshot" << std::endl;
        return g_cachedScreenshot;
    }
    g_cachedScreenshot = CaptureScreenBase64();
    g_cachedScreenshotTime = now;
    return g_cachedScreenshot;
}

void InvalidateScreenshotCache() {
    g_cachedScreenshot.clear();
    g_cachedScreenshotTime = 0;
}

// ★ 地址栏 / URL 直接走快捷键，省一次 AI 请求
// "地址栏" UTF-8 = E5 9C B0 E5 9D 80 E6 A0 8F
static bool TryQuickAddressBar(const std::string& hint) {
    std::string h = hint;
    for (auto& c : h) c = tolower(c);
    const std::string CN_ADDRESS_BAR = "\xE5\x9C\xB0\xE5\x9D\x80\xE6\xA0\x8F";
    if (h == "address" || h == "url" || h == CN_ADDRESS_BAR ||
        h.find("address") != std::string::npos) {
        std::cout << "[VISION] address bar -> Ctrl+L" << std::endl;
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('L', 0, 0, 0);
        keybd_event('L', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        return true;
    }
    return false;
}

// ========== 找桌面图标 ==========
bool FindIconByAI(const std::string& label, int& outX, int& outY) {
    if (IsStopRequested()) return false;

    std::string imgB64 = GetCachedScreenshot();
    if (imgB64.empty()) return false;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Windows desktop screenshot, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the DESKTOP ICON whose label is: " + label + "\n\n";
    prompt += "Desktop icons: PICTURE on top, TEXT below. Give coordinates of PICTURE CENTER.\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    if (IsStopRequested()) return false;
    if (reply.find("ERROR:") == 0) return false;

    if (reply.find("CLICK") == 0) {
        int x = 0, y = 0;
        if (sscanf_s(reply.c_str(), "CLICK %d %d", &x, &y) == 2) {
            if (x >= 0 && x < sw && y >= 0 && y < sh) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }
    return false;
}

// ========== 找菜单项 / 按钮 ==========
bool FindMenuTextByAI(const std::string& label, int& outX, int& outY) {
    if (IsStopRequested()) return false;

    if (FindClickableByUIA(U2W(label), outX, outY)) return true;

    std::string imgB64 = GetCachedScreenshot();
    if (imgB64.empty()) return false;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Screenshot of Windows, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the element (button/menu item) whose text is: \"" + label + "\"\n\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    if (IsStopRequested()) return false;
    if (reply.find("ERROR:") == 0) return false;

    if (reply.find("CLICK") == 0) {
        int x = 0, y = 0;
        if (sscanf_s(reply.c_str(), "CLICK %d %d", &x, &y) == 2) {
            if (x >= 0 && x < sw && y >= 0 && y < sh) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }
    return false;
}

// ========== 找输入框 ==========
bool FindInputByAI(const std::string& hint, int& outX, int& outY) {
    if (IsStopRequested()) return false;

    // ① UIA 优先
    if (FocusInputByUIA(U2W(hint))) {
        outX = -1;
        outY = -1;
        return true;
    }

    // ② 快捷键优先（address / url）
    if (TryQuickAddressBar(hint)) {
        outX = -1;
        outY = -1;
        return true;
    }

    // ③ AI 兜底
    std::string imgB64 = GetCachedScreenshot();
    if (imgB64.empty()) return false;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Screenshot of Windows, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the INPUT BOX named: \"" + hint + "\"\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    if (IsStopRequested()) return false;
    if (reply.find("ERROR:") == 0) return false;

    if (reply.find("CLICK") == 0) {
        int x = 0, y = 0;
        if (sscanf_s(reply.c_str(), "CLICK %d %d", &x, &y) == 2) {
            if (x >= 0 && x < sw && y >= 0 && y < sh) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }
    return false;
}
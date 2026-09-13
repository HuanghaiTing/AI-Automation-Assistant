#include "vision.h"
#include "ai.h"
#include "uia.h"
#include <iostream>

// ========== 找桌面图标（AI 视觉兜底）==========
bool FindIconByAI(const std::string& label, int& outX, int& outY) {
    std::string imgB64 = CaptureScreenBase64();
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Windows desktop screenshot, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the DESKTOP ICON whose label is: " + label + "\n\n";
    prompt += "Desktop icons: PICTURE on top, TEXT below. Give coordinates of PICTURE CENTER.\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    std::cout << "[VISION-ICON] " << reply << std::endl;

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

// ========== 找菜单项 / 按钮（UIA 优先，AI 兜底）==========
bool FindMenuTextByAI(const std::string& label, int& outX, int& outY) {
    // ① 先试 UIA
    if (FindClickableByUIA(U2W(label), outX, outY)) return true;

    // ② AI 视觉兜底
    std::string imgB64 = CaptureScreenBase64();
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Screenshot of Windows, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the element (button/menu item) whose text is: \"" + label + "\"\n\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    std::cout << "[MENU-ICON] " << reply << std::endl;

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

// ========== 找输入框（UIA 优先，AI 兜底）==========
bool FindInputByAI(const std::string& hint, int& outX, int& outY) {
    // ① 先试 UIA（SetFocus）
    if (FocusInputByUIA(U2W(hint))) {
        outX = -1;
        outY = -1;
        return true;
    }

    // ② AI 视觉兜底
    std::string imgB64 = CaptureScreenBase64();
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::string prompt;
    prompt += "Screenshot of Windows, resolution " + std::to_string(sw) + "x" + std::to_string(sh) + ".\n\n";
    prompt += "Find the INPUT BOX named: \"" + hint + "\"\n";
    prompt += "Reply ONLY: CLICK <x> <y>\n";
    prompt += "Not found: NONE";

    std::string reply = CallAIVision(prompt, imgB64);
    std::cout << "[INPUT-ICON] " << reply << std::endl;

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
#include "input.h"
#include <iostream>

void PressHotkeyString(const std::wstring& hotkey) {
    if (hotkey == L"win") {
        keybd_event(VK_LWIN, 0, 0, 0);
        Sleep(50);
        keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
        return;
    }
    std::vector<WORD> keys;
    std::wstring cur;
    for (size_t i = 0; i <= hotkey.size(); i++) {
        if (i == hotkey.size() || hotkey[i] == L'+') {
            if (cur == L"win") keys.push_back(VK_LWIN);
            else if (cur == L"ctrl") keys.push_back(VK_CONTROL);
            else if (cur == L"shift") keys.push_back(VK_SHIFT);
            else if (cur == L"alt") keys.push_back(VK_MENU);
            else if (cur == L"esc") keys.push_back(VK_ESCAPE);
            else if (cur == L"enter") keys.push_back(VK_RETURN);
            else if (cur == L"tab") keys.push_back(VK_TAB);
            else if (cur.size() == 1) {
                wchar_t c = towupper(cur[0]);
                keys.push_back((WORD)c);
            }
            cur.clear();
        }
        else cur += hotkey[i];
    }
    for (size_t i = 0; i < keys.size(); i++) keybd_event((BYTE)keys[i], 0, 0, 0);
    Sleep(50);
    for (int i = (int)keys.size() - 1; i >= 0; i--) keybd_event((BYTE)keys[i], 0, KEYEVENTF_KEYUP, 0);
    std::cout << "[EXEC] Hotkey done" << std::endl;
}

void DoubleClickAt(int x, int y) {
    SetCursorPos(x, y);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(80);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    std::cout << "[EXEC] DoubleClick (" << x << ", " << y << ")" << std::endl;
}

void SingleClickAt(int x, int y) {
    SetCursorPos(x, y);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(30);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    std::cout << "[EXEC] Click (" << x << ", " << y << ")" << std::endl;
}

void RightClickAt(int x, int y) {
    SetCursorPos(x, y);
    Sleep(50);
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    Sleep(30);
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
    std::cout << "[EXEC] RightClick (" << x << ", " << y << ")" << std::endl;
}

void TypeText(const std::wstring& text) {
    if (text.empty()) return;
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
    Sleep(150);
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    std::cout << "[EXEC] Typed: " << W2U(text) << std::endl;
}

void PressKeyByName(const std::string& key) {
    std::string k = key;
    for (auto& c : k) c = tolower(c);

    if (k == "enter") { keybd_event(VK_RETURN, 0, 0, 0); keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0); }
    else if (k == "tab") { keybd_event(VK_TAB, 0, 0, 0); keybd_event(VK_TAB, 0, KEYEVENTF_KEYUP, 0); }
    else if (k == "esc") { keybd_event(VK_ESCAPE, 0, 0, 0); keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0); }
    else if (k == "space") { keybd_event(VK_SPACE, 0, 0, 0); keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0); }
    else if (k == "win") {
        keybd_event(VK_LWIN, 0, 0, 0);
        Sleep(50);
        keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    }
    else if (k.find("win+") == 0 && k.size() > 4) {
        char c = k[4];
        keybd_event(VK_LWIN, 0, 0, 0);
        Sleep(30);
        keybd_event(toupper(c), 0, 0, 0);
        keybd_event(toupper(c), 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    }
    else if (k.find("ctrl+") == 0 && k.size() > 5) {
        char c = k[5];
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event(toupper(c), 0, 0, 0);
        keybd_event(toupper(c), 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
    else if (k.find("alt+") == 0 && k.size() > 4) {
        char c = k[4];
        keybd_event(VK_MENU, 0, 0, 0);
        keybd_event(toupper(c), 0, 0, 0);
        keybd_event(toupper(c), 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
    }
    else if (k.size() == 1) {
        char c = toupper(k[0]);
        keybd_event(c, 0, 0, 0);
        keybd_event(c, 0, KEYEVENTF_KEYUP, 0);
    }
    std::cout << "[EXEC] Key: " << key << std::endl;
}
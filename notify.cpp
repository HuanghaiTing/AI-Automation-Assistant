#include "notify.h"
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_ICON_ID 1

static HWND g_notifyHwnd = nullptr;
static NOTIFYICONDATAW g_nid = { 0 };
static bool g_notifyReady = false;

bool InitNotify(HWND hwnd) {
    g_notifyHwnd = hwnd;

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"AI Automation Assistant");

    if (!Shell_NotifyIconW(NIM_ADD, &g_nid)) {
        return false;
    }

    g_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_nid);

    g_notifyReady = true;
    return true;
}

void Notify(const std::wstring& title, const std::wstring& message) {
    if (!g_notifyReady) return;

    g_nid.uFlags = NIF_INFO;
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 10000;

    wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szInfo, message.c_str(), _TRUNCATE);
    wcsncpy_s(g_nid.szTip, L"AI Automation Assistant", _TRUNCATE);

    Shell_NotifyIconW(NIM_MODIFY, &g_nid);

    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

void ShutdownNotify() {
    if (g_notifyReady) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_notifyReady = false;
    }
}
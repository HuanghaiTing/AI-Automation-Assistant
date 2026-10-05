#include "panel_host.h"
#include "logger.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <wrl.h>
#include <WebView2.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>

#include "C:\Users\Administrator\Downloads\json.hpp"

#pragma comment(lib, "dwmapi.lib")

using json = nlohmann::json;
using namespace Microsoft::WRL;

static HWND  g_panelHwnd = nullptr;
static HINSTANCE g_panelInst = nullptr;
static std::wstring g_panelHtmlPath;
static ComPtr<ICoreWebView2Controller> g_panelController;
static ComPtr<ICoreWebView2>           g_panelWebView;
static std::mutex g_panelMutex;
static std::atomic<bool> g_panelReady{ false };
static std::atomic<bool> g_panelVisible{ false };

static const int PANEL_WIDTH = 400;
static const int PANEL_HEIGHT = 640;
static const int PANEL_MARGIN = 16;

static std::string WideToUtf8Local(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWideLocal(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

extern void RequestStopFromPanel();

static void UpdatePanelBounds() {
    if (!g_panelController) return;
    if (!g_panelHwnd) return;

    RECT rc;
    GetClientRect(g_panelHwnd, &rc);
    int wPx = rc.right - rc.left;
    int hPx = rc.bottom - rc.top;
    if (wPx <= 0 || hPx <= 0) return;

    UINT dpi = GetDpiForWindow(g_panelHwnd);
    if (dpi == 0) dpi = 96;
    float scale = (float)dpi / 96.0f;

    RECT bounds;
    bounds.left = 0;
    bounds.top = 0;
    bounds.right = (LONG)(wPx / scale);
    bounds.bottom = (LONG)(hPx / scale);

    g_panelController->put_Bounds(bounds);
}

static HRESULT OnPanelMessage(ICoreWebView2*,
    ICoreWebView2WebMessageReceivedEventArgs* args)
{
    LPWSTR raw = nullptr;
    if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;

    std::wstring wraw(raw);
    CoTaskMemFree(raw);

    std::string msgUtf8 = WideToUtf8Local(wraw);

    try {
        json j = json::parse(msgUtf8);
        std::string type = j.value("type", "");

        if (type == "panelStop") {
            LogInfo("[PANEL] user clicked Stop");
            RequestStopFromPanel();
        }
        else if (type == "panelHide") {
            LogInfo("[PANEL] user closed panel");
            HideControlPanel();
        }
        else if (type == "panelDrag") {
            if (g_panelHwnd) {
                ReleaseCapture();
                PostMessage(g_panelHwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
    }
    catch (...) {}
    return S_OK;
}

static LRESULT CALLBACK PanelWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        UpdatePanelBounds();
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hbr = CreateSolidBrush(RGB(28, 28, 30));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH hbr = CreateSolidBrush(RGB(28, 28, 30));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        return 1;
    }

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (pt.y >= 0 && pt.y < 48) {
            if (pt.x > rc.right - 40) return HTCLIENT;
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_CLOSE:
        HideControlPanel();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static RECT CalcPanelRect() {
    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    float scale = (float)dpi / 96.0f;

    int wPx = (int)(PANEL_WIDTH * scale);
    int hPx = (int)(PANEL_HEIGHT * scale);
    int mPx = (int)(PANEL_MARGIN * scale);

    int x = workArea.right - wPx - mPx;
    int y = workArea.top + mPx;

    if (x < 0) x = 0;

    RECT r;
    r.left = x;
    r.top = y;
    r.right = x + wPx;
    r.bottom = y + hPx;
    return r;
}

bool InitPanelHost(HINSTANCE hInst, const std::wstring& htmlPath) {
    g_panelInst = hInst;
    g_panelHtmlPath = htmlPath;

    LogInfo("[PANEL] InitPanelHost, path=" + WideToUtf8Local(htmlPath));

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t* clsName = L"AIControlPanelWindow";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = PanelWndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = clsName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(28, 28, 30));
    if (!RegisterClassW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            LogError("[PANEL] RegisterClass failed, err=" + std::to_string(err));
            return false;
        }
    }

    RECT r = CalcPanelRect();

    // ★ WS_EX_LAYERED 支持滑动时的不透明控制
    g_panelHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        clsName, L"AI Control Center",
        WS_POPUP,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInst, nullptr);

    if (!g_panelHwnd) {
        LogError("[PANEL] CreateWindowEx failed, err=" + std::to_string(GetLastError()));
        return false;
    }

    int cornerPref = 2;
    DwmSetWindowAttribute(g_panelHwnd, 33, &cornerPref, sizeof(cornerPref));

    ShowWindow(g_panelHwnd, SW_HIDE);

    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    std::wstring userData = std::wstring(temp) + L"Project4PanelWebView2";

    auto envCallback = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(hr) || !env) {
                LogError("[PANEL] CreateCoreWebView2Environment failed, hr=" +
                    std::to_string(hr));
                return S_OK;
            }
            LogInfo("[PANEL] Environment ready");

            env->CreateCoreWebView2Controller(g_panelHwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [](HRESULT hr2, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(hr2) || !controller) {
                            LogError("[PANEL] CreateCoreWebView2Controller failed, hr=" +
                                std::to_string(hr2));
                            return S_OK;
                        }
                        g_panelController = controller;
                        controller->get_CoreWebView2(&g_panelWebView);
                        LogInfo("[PANEL] Controller ready");

                        UpdatePanelBounds();

                        ComPtr<ICoreWebView2Settings> settings;
                        g_panelWebView->get_Settings(&settings);
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(TRUE);

                        ComPtr<ICoreWebView2Controller2> ctrl2;
                        if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2)))) {
                            COREWEBVIEW2_COLOR bg = { 0, 0, 0, 0 };
                            ctrl2->put_DefaultBackgroundColor(bg);
                        }

                        EventRegistrationToken navToken;
                        g_panelWebView->add_NavigationCompleted(
                            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                    BOOL success = FALSE;
                                    args->get_IsSuccess(&success);
                                    COREWEBVIEW2_WEB_ERROR_STATUS errStatus = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                                    args->get_WebErrorStatus(&errStatus);
                                    LogInfo("[PANEL] NavigationCompleted success=" +
                                        std::to_string(success ? 1 : 0) +
                                        " errStatus=" + std::to_string((int)errStatus));
                                    return S_OK;
                                }).Get(), &navToken);

                        g_panelWebView->add_WebMessageReceived(
                            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                OnPanelMessage).Get(), nullptr);

                        HRESULT hrNav = g_panelWebView->Navigate(g_panelHtmlPath.c_str());
                        LogInfo("[PANEL] Navigate hr=" + std::to_string(hrNav));

                        g_panelReady = true;
                        LogInfo("[PANEL] WebView2 initialized");
                        return S_OK;
                    }).Get());
            return S_OK;
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr, envCallback.Get());
    if (FAILED(hr)) {
        LogError("[PANEL] CreateCoreWebView2EnvironmentWithOptions failed, hr=" +
            std::to_string(hr));
        return false;
    }

    return true;
}

// ★ 面板从右侧滑入
void ShowControlPanel(const std::wstring& taskText) {
    if (!g_panelHwnd) {
        LogError("[PANEL] ShowControlPanel: no hwnd");
        return;
    }

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    float scale = (float)dpi / 96.0f;

    int wPx = (int)(PANEL_WIDTH * scale);
    int hPx = (int)(PANEL_HEIGHT * scale);
    int mPx = (int)(PANEL_MARGIN * scale);

    int targetX = workArea.right - wPx - mPx;
    int startX = workArea.right;   // 从屏幕外滑入
    int y = workArea.top + mPx;

    if (targetX < 0) targetX = 0;

    // 先设置到屏幕外
    SetWindowPos(g_panelHwnd, HWND_TOPMOST,
        startX, y, wPx, hPx,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    // ★ 滑动动画：从 startX 到 targetX
    const int STEPS = 18;
    for (int i = 0; i <= STEPS; i++) {
        int x = startX + (targetX - startX) * i / STEPS;
        SetWindowPos(g_panelHwnd, HWND_TOPMOST,
            x, y, wPx, hPx,
            SWP_NOACTIVATE);
        Sleep(8);
    }

    UpdatePanelBounds();

    json j;
    j["type"] = "panelShow";
    j["task"] = WideToUtf8Local(taskText);
    PostToPanel(j.dump());

    g_panelVisible = true;
    LogInfo("[PANEL] shown");
}

// ★ 面板向右滑出
void HideControlPanel() {
    if (!g_panelHwnd) return;

    RECT workArea;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    float scale = (float)dpi / 96.0f;

    int wPx = (int)(PANEL_WIDTH * scale);
    int hPx = (int)(PANEL_HEIGHT * scale);
    int mPx = (int)(PANEL_MARGIN * scale);

    int startX = workArea.right - wPx - mPx;
    int endX = workArea.right;   // 滑到屏幕外
    int y = workArea.top + mPx;

    const int STEPS = 18;
    for (int i = 0; i <= STEPS; i++) {
        int x = startX + (endX - startX) * i / STEPS;
        SetWindowPos(g_panelHwnd, HWND_TOPMOST,
            x, y, wPx, hPx,
            SWP_NOACTIVATE);
        Sleep(8);
    }

    ShowWindow(g_panelHwnd, SW_HIDE);
    g_panelVisible = false;
}

void PostToPanel(const std::string& jsonUtf8) {
    if (!g_panelReady) return;
    if (!g_panelWebView) return;

    std::wstring w = Utf8ToWideLocal(jsonUtf8);
    g_panelWebView->PostWebMessageAsJson(w.c_str());
}

void ShutdownPanelHost() {
    if (g_panelController) g_panelController->Close();
    g_panelController.Reset();
    g_panelWebView.Reset();
    if (g_panelHwnd) {
        DestroyWindow(g_panelHwnd);
        g_panelHwnd = nullptr;
    }
}

HWND GetPanelHwnd() {
    return g_panelHwnd;
}
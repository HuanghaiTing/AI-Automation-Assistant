#include "webview_host.h"
#include "logger.h"
#include "config.h"
#include "i18n.h"
#include "file_picker.h"
#include "panel_host.h"

#include <wrl.h>
#include <WebView2.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include <iostream>

#include "C:\Users\Administrator\Downloads\json.hpp"

#pragma comment(lib, "user32.lib")

using namespace Microsoft::WRL;
using json = nlohmann::json;

#ifndef WM_PICK_FILES
#define WM_PICK_FILES  (WM_USER + 100)
#endif
#ifndef WM_PICK_FOLDER
#define WM_PICK_FOLDER (WM_USER + 101)
#endif
#ifndef WM_SEND_TO_JS
#define WM_SEND_TO_JS  (WM_USER + 102)
#endif

// ★ 前置声明：main.cpp 里的 ApplyBackdrop
extern void ApplyBackdrop(HWND hwnd, const std::string& mode);

// ========== 全局状态 ==========
static HWND g_hwnd = nullptr;
static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2>           g_webview;
static TaskCallback g_taskCb;
static StopCallback g_stopCb;
static std::mutex g_postMutex;

std::vector<std::string> g_attachedPaths;

static std::vector<std::string> g_logQueue;
static std::mutex g_logQueueMutex;

// ========== UTF-8 / Wide 转换 ==========
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string JsonEscape(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"':  r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n";  break;
        case '\r': r += "\\r";  break;
        case '\t': r += "\\t";  break;
        default:
            if ((unsigned char)c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                r += buf;
            }
            else r += c;
        }
    }
    return r;
}

void PostToJS(const std::string& jsonUtf8) {
    if (g_hwnd && GetCurrentThreadId() != GetWindowThreadProcessId(g_hwnd, nullptr)) {
        PostToJSFromAnyThread(jsonUtf8);
        return;
    }

    std::lock_guard<std::mutex> lock(g_postMutex);
    if (!g_webview) return;
    std::wstring w = Utf8ToWide(jsonUtf8);
    g_webview->PostWebMessageAsJson(w.c_str());
}

void PostToJSFromAnyThread(const std::string& jsonUtf8) {
    if (!g_hwnd) return;
    std::string* pMsg = new std::string(jsonUtf8);
    if (!PostMessage(g_hwnd, WM_SEND_TO_JS, 0, (LPARAM)pMsg)) {
        delete pMsg;
    }
}

void HandleSendToJS(LPARAM lp) {
    std::string* pMsg = reinterpret_cast<std::string*>(lp);
    if (pMsg) {
        PostToJS(*pMsg);
        delete pMsg;
    }
}

void PushLogToFrontend(const std::string& text) {
    std::string jsonStr = "{\"type\":\"log\",\"level\":\"info\",\"text\":\"" +
        JsonEscape(text) + "\"}";

    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(g_postMutex);
        ready = (g_webview != nullptr);
    }

    if (!ready) {
        std::lock_guard<std::mutex> lock(g_logQueueMutex);
        g_logQueue.push_back(jsonStr);
        return;
    }

    PostToJSFromAnyThread(jsonStr);
    PostToPanel(jsonStr);
}

void FlushPendingLogs() {
    std::vector<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(g_logQueueMutex);
        pending.swap(g_logQueue);
    }
    for (auto& j : pending) {
        PostToJSFromAnyThread(j);
        PostToPanel(j);
    }
}

void NotifyPickedItems(const std::vector<PickedItem>& items) {
    json arr = json::array();
    for (auto& it : items) {
        json o;
        o["type"] = it.type;
        o["path"] = it.path;
        o["name"] = it.name;
        arr.push_back(o);
    }
    json resp;
    resp["type"] = "pickedItems";
    resp["items"] = arr;
    PostToJSFromAnyThread(resp.dump());
}

static void BindBridgeCallbacks() {
    g_bridge.onDone = [](bool success, const std::string& reason) {
        json j;
        j["type"] = "done";
        j["success"] = success;
        j["reason"] = reason;
        PostToJSFromAnyThread(j.dump());
        PostToPanel(j.dump());
        };

    g_bridge.onStep = [](int cur, int total) {
        json j;
        j["type"] = "step";
        j["current"] = cur;
        j["total"] = total;
        PostToJSFromAnyThread(j.dump());
        PostToPanel(j.dump());
        };

    g_bridge.onAIError = [](const std::string& type, const std::string& message) {
        json j;
        j["type"] = "aiError";
        j["errType"] = type;
        j["message"] = message;
        PostToJSFromAnyThread(j.dump());
        PostToPanel(j.dump());
        };

    g_bridge.onScreenshot = [](const std::string& b64) {
        json j;
        j["type"] = "screenshot";
        j["b64"] = b64;
        PostToJSFromAnyThread(j.dump());
        };

    g_bridge.onLog = [](LogLevel lv, const std::string& msg) {
        };
}

static HRESULT OnWebMessageReceived(ICoreWebView2*,
    ICoreWebView2WebMessageReceivedEventArgs* args) {
    LPWSTR raw = nullptr;
    if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;

    std::wstring wraw(raw);
    CoTaskMemFree(raw);

    std::string msgUtf8 = WideToUtf8(wraw);

    try {
        json j = json::parse(msgUtf8);
        std::string type = j.value("type", "");
        LogInfo("Received: " + type);

        if (type == "runTask") {
            std::string task = j.value("task", "");
            if (g_taskCb && !task.empty()) g_taskCb(task);
        }
        else if (type == "stop") {
            if (g_stopCb) g_stopCb();
        }
        else if (type == "attachImage") {
            std::string name = j.value("name", "image.png");
            std::string b64 = j.value("b64", "");
            extern void SetAttachedImage(const std::string & name, const std::string & b64);
            SetAttachedImage(name, b64);
            LogInfo("Image attached: " + name + " (" + std::to_string(b64.size()) + " bytes)");
        }
        else if (type == "winMin") {
            if (g_hwnd) {
                AnimateWindow(g_hwnd, 150, AW_HIDE | AW_BLEND);
                ShowWindow(g_hwnd, SW_MINIMIZE);
            }
        }
        else if (type == "winMax") {
            if (g_hwnd) {
                if (IsZoomed(g_hwnd)) {
                    ShowWindow(g_hwnd, SW_RESTORE);
                    AnimateWindow(g_hwnd, 180, AW_ACTIVATE | AW_BLEND);
                }
                else {
                    AnimateWindow(g_hwnd, 120, AW_HIDE | AW_BLEND);
                    ShowWindow(g_hwnd, SW_MAXIMIZE);
                    AnimateWindow(g_hwnd, 150, AW_ACTIVATE | AW_BLEND);
                }
            }
        }
        else if (type == "winClose") {
            if (g_stopCb) g_stopCb();
            if (g_hwnd) PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        }
        else if (type == "winDrag") {
            if (g_hwnd) {
                ReleaseCapture();
                PostMessage(g_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        else if (type == "pickFiles") {
            if (g_hwnd) PostMessage(g_hwnd, WM_PICK_FILES, 0, 0);
        }
        else if (type == "pickFolder") {
            if (g_hwnd) PostMessage(g_hwnd, WM_PICK_FOLDER, 0, 0);
        }
        else if (type == "setAttachments") {
            g_attachedPaths.clear();
            if (j.contains("paths") && j["paths"].is_array()) {
                for (auto& p : j["paths"]) {
                    g_attachedPaths.push_back(p.get<std::string>());
                }
            }
            LogInfo("Attachments: " + std::to_string(g_attachedPaths.size()));
        }
        else if (type == "getConfig") {
            json resp;
            resp["type"] = "config";
            resp["provider"] = g_config.provider;
            resp["api_key"] = g_config.api_key;
            resp["base_url"] = g_config.base_url;
            resp["model"] = g_config.model;
            resp["max_steps"] = g_config.max_steps;
            resp["language"] = g_config.language;
            resp["backdrop"] = g_config.backdrop;   // ★
            PostToJS(resp.dump());
        }
        else if (type == "saveConfig") {
            std::string oldBackdrop = g_config.backdrop;

            g_config.provider = j.value("provider", g_config.provider);
            g_config.api_key = j.value("api_key", g_config.api_key);
            g_config.base_url = j.value("base_url", g_config.base_url);
            g_config.model = j.value("model", g_config.model);
            g_config.max_steps = j.value("max_steps", g_config.max_steps);
            g_config.language = j.value("language", g_config.language);
            g_config.backdrop = j.value("backdrop", g_config.backdrop);   // ★

            bool ok = SaveConfig(g_config);
            InitI18n(g_config.language);

            // ★ 如果 backdrop 变了，重新应用材质
            if (oldBackdrop != g_config.backdrop && g_hwnd) {
                ApplyBackdrop(g_hwnd, g_config.backdrop);
            }

            LogInfo("Config saved (ok=" + std::string(ok ? "1" : "0") + ")");
            json ack;
            ack["type"] = "configSaved";
            ack["ok"] = ok;
            PostToJS(ack.dump());
        }
    }
    catch (const std::exception& e) {
        LogError(std::string("JSON parse error: ") + e.what());
    }
    return S_OK;
}

bool InitWebView2(HWND hwnd, const std::wstring& htmlPath) {
    g_hwnd = hwnd;

    BindBridgeCallbacks();

    wchar_t temp[MAX_PATH];
    GetTempPathW(MAX_PATH, temp);
    std::wstring userData = std::wstring(temp) + L"Project4WebView2";

    auto envCallback = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [htmlPath](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(hr) || !env) {
                LogError("CreateCoreWebView2Environment failed");
                return S_OK;
            }
            env->CreateCoreWebView2Controller(g_hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [htmlPath](HRESULT hr2, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(hr2) || !controller) {
                            LogError("CreateCoreWebView2Controller failed");
                            return S_OK;
                        }
                        g_controller = controller;
                        controller->get_CoreWebView2(&g_webview);

                        RECT rc;
                        GetClientRect(g_hwnd, &rc);
                        controller->put_Bounds(rc);

                        ComPtr<ICoreWebView2Settings> settings;
                        g_webview->get_Settings(&settings);
                        settings->put_AreDefaultContextMenusEnabled(FALSE);
                        settings->put_IsStatusBarEnabled(FALSE);
                        settings->put_AreDevToolsEnabled(TRUE);

                        // ★ 让 WebView2 背景透明，DWM 材质才能透出来
                        ComPtr<ICoreWebView2Controller2> ctrl2;
                        if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&ctrl2)))) {
                            COREWEBVIEW2_COLOR bg = { 0, 0, 0, 0 };
                            ctrl2->put_DefaultBackgroundColor(bg);
                        }

                        g_webview->add_WebMessageReceived(
                            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                OnWebMessageReceived).Get(), nullptr);

                        g_webview->Navigate(htmlPath.c_str());

                        LogInfo("WebView2 initialized");

                        std::thread([]() {
                            Sleep(2000);
                            FlushPendingLogs();
                            LogInfo("Log pipeline ready");
                            }).detach();

                        return S_OK;
                    }).Get());
            return S_OK;
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.c_str(), nullptr, envCallback.Get());
    return SUCCEEDED(hr);
}

void ResizeWebView(HWND hwnd) {
    if (!g_controller) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    g_controller->put_Bounds(rc);
}

void SetTaskCallback(TaskCallback cb) {
    g_taskCb = std::move(cb);
    LogInfo("TaskCallback set");
}

void SetStopCallback(StopCallback cb) {
    g_stopCb = std::move(cb);
    LogInfo("StopCallback set");
}

void ShutdownWebView2() {
    std::lock_guard<std::mutex> lock(g_postMutex);
    if (g_controller) g_controller->Close();
    g_controller.Reset();
    g_webview.Reset();
}
#include "log_tailer.h"
#include "logger.h"
#include "webview_host.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <atomic>
#include <thread>

static std::atomic<bool> g_running{ false };
static std::thread g_thread;

// 和 logger.cpp 用同一个路径
static std::wstring GetLogFilePath() {
    return L"C:\\Users\\Administrator\\Desktop\\1\\app.log";
}

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

// JSON 转义
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
            else {
                r += c;
            }
        }
    }
    return r;
}

static void TailLoop() {
    std::string pathUtf8 = WideToUtf8(GetLogFilePath());
    std::streampos lastPos = 0;

    // 首次：如果文件已存在，从末尾开始（不重发旧日志）
    {
        std::ifstream f(pathUtf8, std::ios::binary);
        if (f.is_open()) {
            f.seekg(0, std::ios::end);
            lastPos = f.tellg();
            f.close();
        }
    }

    while (g_running) {
        std::ifstream f(pathUtf8, std::ios::binary);
        if (f.is_open()) {
            f.seekg(0, std::ios::end);
            std::streampos size = f.tellg();

            if (size < lastPos) {
                // 文件被清空 / 截断
                lastPos = 0;
            }

            if (size > lastPos) {
                f.seekg(lastPos);
                std::string line;
                while (std::getline(f, line)) {
                    if (line.empty()) continue;
                    std::string json = "{\"type\":\"log\",\"level\":\"info\",\"text\":\"" +
                        JsonEscape(line) + "\"}";
                    PostToJS(json);
                }
                lastPos = f.tellg();
            }
            f.close();
        }
        Sleep(300);
    }
}

void StartLogTailer() {
    if (g_running) return;
    g_running = true;
    g_thread = std::thread(TailLoop);
}

void StopLogTailer() {
    if (!g_running) return;
    g_running = false;
    if (g_thread.joinable()) g_thread.join();
}
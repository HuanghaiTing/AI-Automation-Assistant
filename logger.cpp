#include "logger.h"
#include "webview_host.h"
#include <iostream>
#include <windows.h>

BridgeCallbacks g_bridge;

void Log(LogLevel lv, const std::string& msg) {
    // 1. 标签
    const char* tag = "";
    switch (lv) {
    case LogLevel::Info:  tag = "[INFO] ";  break;
    case LogLevel::Exec:  tag = "[EXEC] ";  break;
    case LogLevel::AI:    tag = "[AI] ";    break;
    case LogLevel::UIA:   tag = "[UIA] ";   break;
    case LogLevel::Error: tag = "[ERROR] "; break;
    case LogLevel::Step:  tag = "[STEP] ";  break;
    }

    // 2. 控制台（Debug 时可见；Release 时被 FreeConsole 静默丢弃）
    std::cout << tag << msg << std::endl;

    // 3. VS 输出窗口（永远可见）
    OutputDebugStringA(tag);
    OutputDebugStringA(msg.c_str());
    OutputDebugStringA("\n");

    // 4. 前端（WebView2 未就绪时自动缓存）
    PushLogToFrontend(std::string(tag) + msg);
}

void FlushLogBuffer() { /* no-op */ }

void LogInfo(const std::string& s) { Log(LogLevel::Info, s); }
void LogExec(const std::string& s) { Log(LogLevel::Exec, s); }
void LogAI(const std::string& s) { Log(LogLevel::AI, s); }
void LogUIA(const std::string& s) { Log(LogLevel::UIA, s); }
void LogError(const std::string& s) { Log(LogLevel::Error, s); }
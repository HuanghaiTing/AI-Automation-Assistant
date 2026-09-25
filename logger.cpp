#include "logger.h"
#include "webview_host.h"
#include <iostream>

BridgeCallbacks g_bridge;

void Log(LogLevel lv, const std::string& msg) {
    // 1. 控制台
    const char* tag = "";
    switch (lv) {
    case LogLevel::Info:  tag = "[INFO] ";  break;
    case LogLevel::Exec:  tag = "[EXEC] ";  break;
    case LogLevel::AI:    tag = "[AI] ";    break;
    case LogLevel::UIA:   tag = "[UIA] ";   break;
    case LogLevel::Error: tag = "[ERROR] "; break;
    case LogLevel::Step:  tag = "[STEP] ";  break;
    }
    std::cout << tag << msg << std::endl;

    // 2. 推给前端（走缓存，WebView2 未就绪也不丢）
    PushLogToFrontend(std::string(tag) + msg);
}

void FlushLogBuffer() { /* no-op */ }

void LogInfo(const std::string& s) { Log(LogLevel::Info, s); }
void LogExec(const std::string& s) { Log(LogLevel::Exec, s); }
void LogAI(const std::string& s) { Log(LogLevel::AI, s); }
void LogUIA(const std::string& s) { Log(LogLevel::UIA, s); }
void LogError(const std::string& s) { Log(LogLevel::Error, s); }
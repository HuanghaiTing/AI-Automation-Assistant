#pragma once
#include <string>
#include <functional>

enum class LogLevel { Info, Exec, AI, UIA, Error, Step };

using LogCallback = std::function<void(LogLevel, const std::string&)>;
using ScreenshotCallback = std::function<void(const std::string& b64)>;
using StepCallback = std::function<void(int current, int total)>;
using DoneCallback = std::function<void(bool success, const std::string& reason)>;
using AIErrorCallback = std::function<void(const std::string& type, const std::string& message)>;

struct BridgeCallbacks {
    LogCallback        onLog = nullptr;
    ScreenshotCallback onScreenshot = nullptr;
    StepCallback       onStep = nullptr;
    DoneCallback       onDone = nullptr;
    AIErrorCallback    onAIError = nullptr;
};

extern BridgeCallbacks g_bridge;

void Log(LogLevel lv, const std::string& msg);
void LogInfo(const std::string& s);
void LogExec(const std::string& s);
void LogAI(const std::string& s);
void LogUIA(const std::string& s);
void LogError(const std::string& s);

void FlushLogBuffer();
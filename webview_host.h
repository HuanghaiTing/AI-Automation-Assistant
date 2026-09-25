#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "file_picker.h"

using TaskCallback = std::function<void(const std::string& task)>;
using StopCallback = std::function<void()>;

bool InitWebView2(HWND hwnd, const std::wstring& htmlPath);
void PostToJS(const std::string& jsonUtf8);
void PostToJSFromAnyThread(const std::string& jsonUtf8);
void HandleSendToJS(LPARAM lp);
void ResizeWebView(HWND hwnd);
void SetTaskCallback(TaskCallback cb);
void SetStopCallback(StopCallback cb);
void ShutdownWebView2();

// ★ 日志推送到前端（线程安全，带缓存）
void PushLogToFrontend(const std::string& text);
void FlushPendingLogs();

extern std::vector<std::string> g_attachedPaths;

void NotifyPickedItems(const std::vector<PickedItem>& items);
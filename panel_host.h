#pragma once
#include <windows.h>
#include <string>

// 初始化控制中心窗口（屏幕右侧滑出的面板）
bool InitPanelHost(HINSTANCE hInst, const std::wstring& htmlPath);

// 显示控制中心（同时设置任务文字）
void ShowControlPanel(const std::wstring& taskText);

// 隐藏控制中心
void HideControlPanel();

// 向控制中心推送 JSON 消息（由 logger / webview_host 调用）
void PostToPanel(const std::string& jsonUtf8);

// 关闭控制中心（退出程序时调）
void ShutdownPanelHost();

// 获取窗口句柄（调试用）
HWND GetPanelHwnd();
#pragma once
#include <windows.h>
#include <string>

bool InitNotify(HWND hwnd);
void Notify(const std::wstring& title, const std::wstring& message);
void ShutdownNotify();
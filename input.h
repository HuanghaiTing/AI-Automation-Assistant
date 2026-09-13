#pragma once
#include "common.h"

void PressHotkeyString(const std::wstring& hotkey);
void DoubleClickAt(int x, int y);
void SingleClickAt(int x, int y);
void RightClickAt(int x, int y);
void TypeText(const std::wstring& text);
void PressKeyByName(const std::string& key);
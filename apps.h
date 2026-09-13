#pragma once
#include "common.h"
#include <string>
#include <vector>

// ★ 桌面快捷方式结构体
struct DesktopShortcut {
    std::wstring displayName;
    std::wstring lnkPath;
};

bool TryBuiltinAction(const std::string& rawInput, const std::wstring& target);
bool TryOpenLocal(const std::wstring& cmd);
std::vector<std::wstring> SearchExeCandidates(const std::wstring& partialName);
int AskUserChoice(const std::string& title, const std::vector<std::string>& options);
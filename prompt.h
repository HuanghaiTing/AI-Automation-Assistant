#pragma once
#include <string>
#include <vector>

struct PromptContext {
    std::string task;
    int step = 0;
    int maxSteps = 0;
    std::vector<std::string> doneSteps;
    std::string openWindows;
    std::string language = "zh";
    std::vector<std::string> attachedPaths;
    std::string desktopFiles;   // ★ 新增：桌面文件列表
};

std::string BuildPrompt(const PromptContext& ctx);
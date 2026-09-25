#pragma once
#include <string>
#include <vector>

struct PromptContext {
    std::string task;
    int step;
    int maxSteps;
    std::vector<std::string> doneSteps;
    std::string openWindows;
    std::string language;
    std::vector<std::string> attachedPaths;
};

std::string BuildPrompt(const PromptContext& ctx);
#pragma once
#include <string>

struct AppConfig {
    std::string provider = "qwen";
    std::string api_key = "";
    std::string base_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    std::string model = "qwen-vl-max";
    int         max_steps = 6;   // ★ 12 → 6
    std::string language = "zh";   // "zh" | "en"
};

AppConfig LoadConfig();
bool SaveConfig(const AppConfig& cfg);

extern AppConfig g_config;
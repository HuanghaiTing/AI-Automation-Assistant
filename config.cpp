#include "config.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include "C:\Users\Administrator\Downloads\json.hpp"

using json = nlohmann::json;

AppConfig g_config;

// ========== Absolute path ==========
static const wchar_t* CONFIG_PATH_W = LR"(C:\Users\Administrator\Desktop\1\config.json)";

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring GetConfigPathW() {
    return std::wstring(CONFIG_PATH_W);
}

static void EnsureConfigDir() {
    std::wstring path = GetConfigPathW();
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring dir = path.substr(0, pos);
    CreateDirectoryW(dir.c_str(), nullptr);
}

static void WriteDefaultFile() {
    std::wstring path = GetConfigPathW();
    json j;
    j["provider"] = "qwen";
    j["api_key"] = "";
    j["base_url"] = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    j["model"] = "qwen-vl-max";
    j["max_steps"] = 12;
    j["language"] = "zh";

    std::ofstream f(WideToUtf8(path));
    if (f.is_open()) {
        f << j.dump(2);
        f.close();
        std::cout << "[CONFIG] Created default config at " << WideToUtf8(path) << std::endl;
    }
}

AppConfig LoadConfig() {
    AppConfig cfg;
    std::wstring path = GetConfigPathW();

    std::wcout << L"[CONFIG] Loading from: " << path << std::endl;

    std::ifstream f(WideToUtf8(path));
    if (!f.is_open()) {
        EnsureConfigDir();
        WriteDefaultFile();
        f.open(WideToUtf8(path));
        if (!f.is_open()) {
            std::cout << "[CONFIG] Cannot open config file" << std::endl;
            return cfg;
        }
    }

    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    try {
        json j = json::parse(ss.str());
        cfg.provider = j.value("provider", cfg.provider);
        cfg.api_key = j.value("api_key", cfg.api_key);
        cfg.base_url = j.value("base_url", cfg.base_url);
        cfg.model = j.value("model", cfg.model);
        cfg.max_steps = j.value("max_steps", cfg.max_steps);
        cfg.language = j.value("language", cfg.language);
    }
    catch (const std::exception& e) {
        std::cout << "[CONFIG] Parse error: " << e.what() << std::endl;
    }

    return cfg;
}

bool SaveConfig(const AppConfig& cfg) {
    std::wstring path = GetConfigPathW();
    std::wcout << L"[CONFIG] Saving to: " << path << std::endl;

    EnsureConfigDir();

    json j;
    j["provider"] = cfg.provider;
    j["api_key"] = cfg.api_key;
    j["base_url"] = cfg.base_url;
    j["model"] = cfg.model;
    j["max_steps"] = cfg.max_steps;
    j["language"] = cfg.language;

    std::ofstream f(WideToUtf8(path));
    if (!f.is_open()) {
        std::cout << "[CONFIG] Cannot write config file" << std::endl;
        return false;
    }
    f << j.dump(2);
    f.close();
    return true;
}
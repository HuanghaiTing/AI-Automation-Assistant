#include "config.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cerrno>
#include <cstring>
#include "C:\Users\Administrator\Downloads\json.hpp"

using json = nlohmann::json;

AppConfig g_config;

static const wchar_t* CONFIG_PATH_W = LR"(C:\Users\Administrator\Desktop\1\config.json)";

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring GetConfigPathW() {
    return std::wstring(CONFIG_PATH_W);
}

static std::string ErrnoStr(int e) {
    char buf[128] = { 0 };
    strerror_s(buf, sizeof(buf), e);
    return std::string(buf);
}

static void EnsureConfigDir() {
    std::wstring path = GetConfigPathW();
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring dir = path.substr(0, pos);
    BOOL ok = CreateDirectoryW(dir.c_str(), nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            std::cout << "[CONFIG] CreateDirectoryW failed, err=" << err
                << " dir=" << WideToUtf8(dir) << std::endl;
        }
    }
}

static void WriteDefaultFile() {
    std::wstring path = GetConfigPathW();
    json j;
    j["provider"] = "qwen";
    j["api_key"] = "";
    j["base_url"] = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    j["model"] = "qwen-vl-max";
    j["max_steps"] = 6;
    j["language"] = "zh";
    j["backdrop"] = "mica";

    std::ofstream f(path);
    if (f.is_open()) {
        f << j.dump(2);
        f.close();
        std::cout << "[CONFIG] Created default config at " << WideToUtf8(path) << std::endl;
    }
    else {
        std::cout << "[CONFIG] Failed to create default config at "
            << WideToUtf8(path) << " errno=" << errno
            << " (" << ErrnoStr(errno) << ")" << std::endl;
    }
}

AppConfig LoadConfig() {
    AppConfig cfg;
    std::wstring path = GetConfigPathW();

    std::cout << "[CONFIG] Loading from: " << WideToUtf8(path) << std::endl;

    std::ifstream f(path);
    if (!f.is_open()) {
        std::cout << "[CONFIG] File not found, creating default..." << std::endl;
        EnsureConfigDir();
        WriteDefaultFile();
        f.open(path);
        if (!f.is_open()) {
            std::cout << "[CONFIG] Still cannot open after creating default! errno="
                << errno << " (" << ErrnoStr(errno) << ")" << std::endl;
            return cfg;
        }
    }

    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    std::string raw = ss.str();
    std::cout << "[CONFIG] Raw content (" << raw.size() << " bytes): "
        << raw.substr(0, 300) << std::endl;

    try {
        json j = json::parse(raw);
        cfg.provider = j.value("provider", cfg.provider);
        cfg.api_key = j.value("api_key", cfg.api_key);
        cfg.base_url = j.value("base_url", cfg.base_url);
        cfg.model = j.value("model", cfg.model);
        cfg.max_steps = j.value("max_steps", cfg.max_steps);
        cfg.language = j.value("language", cfg.language);
        cfg.backdrop = j.value("backdrop", cfg.backdrop);

        std::cout << "[CONFIG] Loaded: provider=" << cfg.provider
            << " model=" << cfg.model
            << " base_url=" << cfg.base_url
            << " has_key=" << (cfg.api_key.empty() ? "no" : "yes")
            << " key_len=" << cfg.api_key.size()
            << " max_steps=" << cfg.max_steps
            << " lang=" << cfg.language
            << " backdrop=" << cfg.backdrop << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "[CONFIG] Parse error: " << e.what() << std::endl;
    }

    return cfg;
}

bool SaveConfig(const AppConfig& cfg) {
    std::wstring path = GetConfigPathW();
    std::cout << "[CONFIG] Saving to: " << WideToUtf8(path) << std::endl;

    EnsureConfigDir();

    json j;
    j["provider"] = cfg.provider;
    j["api_key"] = cfg.api_key;
    j["base_url"] = cfg.base_url;
    j["model"] = cfg.model;
    j["max_steps"] = cfg.max_steps;
    j["language"] = cfg.language;
    j["backdrop"] = cfg.backdrop;

    std::string dump = j.dump(2);

    std::ofstream f(path);
    if (!f.is_open()) {
        std::cout << "[CONFIG] Cannot write config file! errno="
            << errno << " (" << ErrnoStr(errno) << ")" << std::endl;
        return false;
    }
    f << dump;
    f.close();

    std::cout << "[CONFIG] Write OK, size=" << dump.size() << " bytes" << std::endl;
    return true;
}
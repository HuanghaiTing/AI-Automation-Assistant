#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <shlobj.h>
#include <shlwapi.h>
#include "../../../Downloads/json.hpp"

using json = nlohmann::json;

// ========== 常量 ==========
extern const std::wstring API_HOST;
extern const std::wstring API_PATH;
extern const std::string API_KEY;
extern const std::string MODEL_VISION;

// ========== UTF8 ==========
std::string W2U(const std::wstring& wstr);
std::wstring U2W(const std::string& str);
std::string CleanUTF8(const std::string& s);
json SafeParseJSON(const std::string& raw);

// ========== 多步检测 ==========
bool IsMultiStep(const std::string& input);
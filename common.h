#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <shlobj.h>
#include <shlwapi.h>
#include "C:\Users\Administrator\Downloads\json.hpp"
#include "config.h"

using json = nlohmann::json;

// UTF8
std::string W2U(const std::wstring& wstr);
std::wstring U2W(const std::string& str);
std::string CleanUTF8(const std::string& s);
json SafeParseJSON(const std::string& raw);

// Multi-step detection
bool IsMultiStep(const std::string& input);
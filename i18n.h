#pragma once
#include <string>

void InitI18n(const std::string& lang);
std::wstring T(const std::string& key);
std::string GetLanguage();
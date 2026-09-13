#pragma once
#include "common.h"

bool FileExists(const std::wstring& path);
std::wstring ExpandPath(const std::wstring& path);
std::wstring FindInRegistry(const std::wstring& exeName);
#pragma once

// windows.h MUST be included first (defines HWND, DWORD, etc.)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>

struct PickedItem {
    std::string type;   // "file" or "folder"
    std::string path;   // absolute path (UTF-8)
    std::string name;   // file/folder name
};

std::vector<PickedItem> PickFiles(HWND parent, bool multiSelect);
std::vector<PickedItem> PickFolder(HWND parent);
std::string FileNameFromPath(const std::string& path);
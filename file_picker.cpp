#include "file_picker.h"   // ← 先 include 自己的头（会先引入 windows.h）

#include <shobjidl.h>
#include <shlwapi.h>
#include <iostream>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

// ========== UTF-8 / Wide 转换 ==========
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// ========== FileNameFromPath ==========
std::string FileNameFromPath(const std::string& path) {
    std::wstring w = Utf8ToWide(path);
    if (w.empty()) return "";
    std::wstring name = PathFindFileNameW(w.c_str());
    return WideToUtf8(name);
}

// ========== PickFiles ==========
std::vector<PickedItem> PickFiles(HWND parent, bool multiSelect) {
    std::vector<PickedItem> result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr) || !pFileOpen) {
        if (needUninit) CoUninitialize();
        return result;
    }

    DWORD options = 0;
    pFileOpen->GetOptions(&options);
    options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multiSelect) options |= FOS_ALLOWMULTISELECT;
    pFileOpen->SetOptions(options);

    hr = pFileOpen->Show(parent);
    if (SUCCEEDED(hr)) {
        IShellItemArray* pItems = nullptr;
        hr = pFileOpen->GetResults(&pItems);
        if (SUCCEEDED(hr) && pItems) {
            DWORD count = 0;
            pItems->GetCount(&count);
            for (DWORD i = 0; i < count; i++) {
                IShellItem* pItem = nullptr;
                if (SUCCEEDED(pItems->GetItemAt(i, &pItem)) && pItem) {
                    PWSTR pszFilePath = nullptr;
                    if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                        PickedItem item;
                        item.type = "file";
                        item.path = WideToUtf8(pszFilePath);
                        item.name = FileNameFromPath(item.path);
                        result.push_back(item);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            }
            pItems->Release();
        }
    }

    pFileOpen->Release();
    if (needUninit) CoUninitialize();
    return result;
}

// ========== PickFolder ==========
std::vector<PickedItem> PickFolder(HWND parent) {
    std::vector<PickedItem> result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&pFileOpen));
    if (FAILED(hr) || !pFileOpen) {
        if (needUninit) CoUninitialize();
        return result;
    }

    DWORD options = 0;
    pFileOpen->GetOptions(&options);
    pFileOpen->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

    hr = pFileOpen->Show(parent);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr) && pItem) {
            PWSTR pszFilePath = nullptr;
            if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                PickedItem item;
                item.type = "folder";
                item.path = WideToUtf8(pszFilePath);
                item.name = FileNameFromPath(item.path);
                result.push_back(item);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
    }

    pFileOpen->Release();
    if (needUninit) CoUninitialize();
    return result;
}
#include "windows_util.h"

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring ExpandPath(const std::wstring& path) {
    wchar_t buf[MAX_PATH];
    DWORD n = ExpandEnvironmentStringsW(path.c_str(), buf, MAX_PATH);
    if (n > 0) return std::wstring(buf, n - 1);
    return path;
}

std::wstring FindInRegistry(const std::wstring& exeName) {
    HKEY hKey;
    std::wstring subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH];
        DWORD size = sizeof(buf);
        if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(buf);
        }
        RegCloseKey(hKey);
    }
    return L"";
}
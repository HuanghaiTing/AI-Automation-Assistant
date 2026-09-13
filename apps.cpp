#include "apps.h"
#include "windows_util.h"
#include "input.h"
#include "uia.h"
#include <iostream>
#include <set>
#include <map>

using namespace std;

// ========== 内置动作表 ==========
struct BuiltinDef {
    string keyUtf8;
    int type;
    string param;
};

vector<BuiltinDef> BuildBuiltins() {
    vector<BuiltinDef> v;
    v.push_back({ "\xE8\xAE\xBE\xE7\xBD\xAE", 0, "win+i" });
    v.push_back({ "\xE6\x8E\xA7\xE5\x88\xB6\xE9\x9D\xA2\xE6\x9D\xBF", 1, "control" });
    v.push_back({ "\xE6\x98\xBE\xE7\xA4\xBA\xE8\xAE\xBE\xE7\xBD\xAE", 2, "ms-settings:display" });
    v.push_back({ "\xE7\xBD\x91\xE7\xBB\x9C\xE8\xAE\xBE\xE7\xBD\xAE", 2, "ms-settings:network" });
    v.push_back({ "\xE5\xA3\xB0\xE9\x9F\xB3\xE8\xAE\xBE\xE7\xBD\xAE", 2, "ms-settings:sound" });
    v.push_back({ "\xE8\x93\x9D\xE7\x89\x99", 2, "ms-settings:bluetooth" });
    v.push_back({ "\xE6\x9B\xB4\xE6\x96\xB0", 2, "ms-settings:windowsupdate" });
    v.push_back({ "\xE4\xBB\xBB\xE5\x8A\xA1\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8", 0, "ctrl+shift+esc" });
    v.push_back({ "\xE8\xB5\x84\xE6\xBA\x90\xE7\x9B\x91\xE8\xA7\x86\xE5\x99\xA8", 1, "resmon" });
    v.push_back({ "\xE6\xB3\xA8\xE5\x86\x8C\xE8\xA1\xA8", 1, "regedit" });
    v.push_back({ "\xE6\x9C\x8D\xE5\x8A\xA1", 1, "services.msc" });
    v.push_back({ "\xE8\xAE\xBE\xE5\xA4\x87\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8", 1, "devmgmt.msc" });
    v.push_back({ "\xE7\xA3\x81\xE7\x9B\x98\xE7\xAE\xA1\xE7\x90\x86", 1, "diskmgmt.msc" });
    v.push_back({ "\xE6\xAD\xA4\xE7\x94\xB5\xE8\x84\x91", 0, "win+e" });
    v.push_back({ "\xE6\x96\x87\xE4\xBB\xB6\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8", 0, "win+e" });
    v.push_back({ "\xE8\xB5\x84\xE6\xBA\x90\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8", 0, "win+e" });
    v.push_back({ "\xE5\x9B\x9E\xE6\x94\xB6\xE7\xAB\x99", 2, "shell:RecycleBinFolder" });
    v.push_back({ "\xE4\xB8\x8B\xE8\xBD\xBD", 2, "shell:Downloads" });
    v.push_back({ "\xE6\x96\x87\xE6\xA1\xA3", 2, "shell:Personal" });
    v.push_back({ "\xE5\x9B\xBE\xE7\x89\x87", 2, "shell:My Pictures" });
    v.push_back({ "cmd", 1, "cmd" });
    v.push_back({ "\xE5\x91\xBD\xE4\xBB\xA4\xE6\x8F\x90\xE7\xA4\xBA\xE7\xAC\xA6", 1, "cmd" });
    v.push_back({ "powershell", 1, "powershell" });
    v.push_back({ "\xE7\xBB\x88\xE7\xAB\xAF", 1, "wt" });
    v.push_back({ "\xE8\xBF\x90\xE8\xA1\x8C", 0, "win+r" });
    v.push_back({ "\xE9\x80\x9A\xE7\x9F\xA5\xE4\xB8\xAD\xE5\xBF\x83", 0, "win+a" });
    v.push_back({ "\xE6\x97\xA5\xE5\x8E\x86", 0, "win+n" });
    v.push_back({ "\xE5\xB0\x8F\xE7\xBB\x84\xE4\xBB\xB6", 0, "win+w" });
    v.push_back({ "\xE4\xBB\xBB\xE5\x8A\xA1\xE8\xA7\x86\xE5\x9B\xBE", 0, "win+tab" });
    v.push_back({ "\xE6\x88\xAA\xE5\x9B\xBE", 0, "win+shift+s" });
    v.push_back({ "\xE9\x94\x81\xE5\xB1\x8F", 0, "win+l" });
    v.push_back({ "\xE6\x98\xBE\xE7\xA4\xBA\xE6\xA1\x8C\xE9\x9D\xA2", 0, "win+d" });
    v.push_back({ "\xE6\x90\x9C\xE7\xB4\xA2", 0, "win+s" });
    v.push_back({ "\xE5\xBC\x80\xE5\xA7\x8B\xE8\x8F\x9C\xE5\x8D\x95", 0, "win" });
    v.push_back({ "\xE8\xAE\xA1\xE7\xAE\x97\xE5\x99\xA8", 1, "calc" });
    v.push_back({ "\xE8\xAE\xB0\xE4\xBA\x8B\xE6\x9C\xAC", 1, "notepad" });
    v.push_back({ "\xE7\x94\xBB\xE5\x9B\xBE", 1, "mspaint" });
    v.push_back({ "\xE4\xBE\xBF\xE7\xAD\xBE", 1, "stikynot" });
    v.push_back({ "\xE6\x94\xBE\xE5\xA4\xA7\xE9\x95\x9C", 1, "magnify" });
    v.push_back({ "\xE5\xB1\x8F\xE5\xB9\x95\xE9\x94\xAE\xE7\x9B\x98", 1, "osk" });
    return v;
}

bool TryBuiltinAction(const string& rawInput, const wstring& target) {
    static vector<BuiltinDef> builtins = BuildBuiltins();
    string targetUtf8 = W2U(target);

    string cleaned = rawInput;
    const char* prefixes[] = { "\xE6\x89\x93\xE5\xBC\x80", "\xE7\x82\xB9\xE5\x87\xBB", "\xE5\x90\xAF\xE5\x8A\xA8",
                                "open ", "click ", "launch " };
    for (int i = 0; i < 6; i++) {
        size_t len = strlen(prefixes[i]);
        if (cleaned.size() >= len && cleaned.compare(0, len, prefixes[i]) == 0) {
            cleaned = cleaned.substr(len);
            break;
        }
    }

    for (size_t i = 0; i < builtins.size(); i++) {
        bool match = false;
        if (rawInput.find(builtins[i].keyUtf8) != string::npos) match = true;
        if (targetUtf8.find(builtins[i].keyUtf8) != string::npos) match = true;
        if (cleaned.find(builtins[i].keyUtf8) != string::npos) match = true;

        if (match) {
            if (builtins[i].type == 0) {
                PressHotkeyString(U2W(builtins[i].param));
                Sleep(1200);
                return true;
            }
            else if (builtins[i].type == 1) {
                ShellExecuteW(nullptr, L"open", U2W(builtins[i].param).c_str(),
                    nullptr, nullptr, SW_SHOWNORMAL);
                cout << "[EXEC] Run: " << builtins[i].param << endl;
                Sleep(1500);
                return true;
            }
            else if (builtins[i].type == 2) {
                ShellExecuteW(nullptr, L"open", U2W(builtins[i].param).c_str(),
                    nullptr, nullptr, SW_SHOWNORMAL);
                cout << "[EXEC] Shell: " << builtins[i].param << endl;
                Sleep(1500);
                return true;
            }
        }
    }
    return false;
}

// ========== 别名表 ==========
struct AppEntry {
    wstring exeName;
    vector<wstring> paths;
};

map<wstring, AppEntry> BuildAppAliases() {
    map<wstring, AppEntry> m;
    m[L"wechat"] = { L"WeChat.exe", { L"C:\\Program Files\\Tencent\\WeChat\\WeChat.exe" } };
    m[L"chrome"] = { L"chrome.exe", { L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe" } };
    m[L"edge"] = { L"msedge.exe", { L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe" } };
    m[L"vscode"] = { L"Code.exe", {
        L"C:\\Program Files\\Microsoft VS Code\\Code.exe",
        L"C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe"
    } };
    return m;
}

// ========== 桌面快捷方式扫描 ==========
vector<DesktopShortcut> ScanDesktopShortcuts() {
    vector<DesktopShortcut> shortcuts;
    wchar_t userDesktop[MAX_PATH] = { 0 };
    wchar_t commonDesktop[MAX_PATH] = { 0 };
    SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, userDesktop);
    SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, commonDesktop);
    wstring dirs[] = { wstring(userDesktop), wstring(commonDesktop) };
    for (int di = 0; di < 2; di++) {
        wstring& dir = dirs[di];
        if (dir.empty()) continue;
        wstring pattern = dir + L"\\*.lnk";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            DesktopShortcut sc;
            sc.lnkPath = dir + L"\\" + fd.cFileName;
            sc.displayName = fd.cFileName;
            if (sc.displayName.size() > 4) {
                wstring ext = sc.displayName.substr(sc.displayName.size() - 4);
                for (auto& c : ext) c = towlower(c);
                if (ext == L".lnk") sc.displayName = sc.displayName.substr(0, sc.displayName.size() - 4);
            }
            shortcuts.push_back(sc);
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return shortcuts;
}

DesktopShortcut* FindDesktopShortcut(const wstring& keyword) {
    static vector<DesktopShortcut> cache;
    if (cache.empty()) cache = ScanDesktopShortcuts();
    wstring lowerKey = keyword;
    for (auto& c : lowerKey) c = towlower(c);
    for (size_t i = 0; i < cache.size(); i++) {
        wstring lowerName = cache[i].displayName;
        for (auto& c : lowerName) c = towlower(c);
        if (lowerName == lowerKey) return &cache[i];
    }
    for (size_t i = 0; i < cache.size(); i++) {
        wstring lowerName = cache[i].displayName;
        for (auto& c : lowerName) c = towlower(c);
        if (lowerName.find(lowerKey) != wstring::npos) return &cache[i];
        if (lowerKey.find(lowerName) != wstring::npos) return &cache[i];
    }
    return nullptr;
}

bool OpenShortcut(const DesktopShortcut& sc) {
    HINSTANCE result = ShellExecuteW(nullptr, L"open", sc.lnkPath.c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)result > 32;
}

// ========== 磁盘搜索 ==========
void SearchDirForExeAll(const wstring& dir, const wstring& lowerPartial, int depth,
    vector<wstring>& results) {
    if (depth <= 0 || dir.size() > 260) return;
    if (results.size() >= 20) return;
    wstring pattern = dir + L"*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        wstring fullPath = dir + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (name == L"Windows" || name == L"$Recycle.Bin") continue;
            SearchDirForExeAll(fullPath + L"\\", lowerPartial, depth - 1, results);
        }
        else {
            wstring lowerName = name;
            for (auto& c : lowerName) c = towlower(c);
            if (lowerName.size() > 4 && lowerName.substr(lowerName.size() - 4) == L".exe") {
                if (lowerName.find(lowerPartial) != wstring::npos) {
                    if (lowerName.find(L"setup") != wstring::npos) continue;
                    if (lowerName.find(L"uninstall") != wstring::npos) continue;
                    results.push_back(fullPath);
                }
            }
        }
        if (results.size() >= 20) break;
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

vector<wstring> SearchExeCandidates(const wstring& partialName) {
    const wchar_t* roots[] = {
        L"C:\\Program Files\\", L"C:\\Program Files (x86)\\",
        L"D:\\Program Files\\", L"D:\\Program Files (x86)\\",
        L"C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\",
        L"C:\\Users\\%USERNAME%\\AppData\\Local\\",
    };
    wstring lowerPartial = partialName;
    for (auto& c : lowerPartial) c = towlower(c);
    vector<wstring> results;
    set<wstring> seen;
    for (int i = 0; i < 6; i++) {
        wstring root = ExpandPath(roots[i]);
        if (!FileExists(root)) continue;
        vector<wstring> tmp;
        SearchDirForExeAll(root, lowerPartial, 3, tmp);
        for (size_t j = 0; j < tmp.size(); j++) {
            if (seen.find(tmp[j]) == seen.end()) {
                seen.insert(tmp[j]);
                results.push_back(tmp[j]);
            }
        }
        if (results.size() >= 20) break;
    }
    return results;
}

int AskUserChoice(const string& title, const vector<string>& options) {
    cout << "\n[CHOICE] " << title << endl;
    for (size_t i = 0; i < options.size(); i++) {
        cout << "  " << (i + 1) << ". " << options[i] << endl;
    }
    cout << "  0. Cancel" << endl;
    cout << "Select: ";
    string line;
    getline(cin, line);
    try {
        int n = stoi(line);
        if (n >= 0 && n <= (int)options.size()) return n;
    }
    catch (...) {}
    return -1;
}

// ========== 判断是不是 QQ ==========
bool IsQQApp(const wstring& path) {
    wstring lower = path;
    for (auto& c : lower) c = towlower(c);
    return lower.find(L"qq.exe") != wstring::npos ||
        lower.find(L"qqnt") != wstring::npos;
}

// ========== 启动程序（QQ 自动加 accessibility）==========
bool LaunchApp(const wstring& path) {
    if (IsQQApp(path)) {
        cout << "[INFO] Launching QQ with accessibility: " << W2U(path) << endl;
        HINSTANCE h = ShellExecuteW(
            nullptr,                              // 1. hwnd
            L"open",                              // 2. operation
            path.c_str(),                         // 3. file
            L"--force-renderer-accessibility",    // 4. parameters
            nullptr,                              // 5. directory
            SW_SHOWNORMAL                         // 6. show
        );
        if ((INT_PTR)h > 32) {
            Sleep(3000);
            return true;
        }
        cout << "[ERROR] Launch failed, code=" << (INT_PTR)h << endl;
    }
    else {
        HINSTANCE h = ShellExecuteW(
            nullptr,                  // 1
            L"open",                  // 2
            path.c_str(),             // 3
            nullptr,                  // 4
            nullptr,                  // 5
            SW_SHOWNORMAL             // 6
        );
        if ((INT_PTR)h > 32) {
            Sleep(2000);
            return true;
        }
        cout << "[ERROR] Launch failed, code=" << (INT_PTR)h << endl;
    }
    return false;
}

// ========== 本地打开 ==========
bool TryOpenLocal(const wstring& cmd) {
    static map<wstring, AppEntry> aliases = BuildAppAliases();
    wstring lower = cmd;
    for (auto& c : lower) c = towlower(c);

    // ① 别名表
    auto it = aliases.find(lower);
    if (it != aliases.end()) {
        for (size_t j = 0; j < it->second.paths.size(); j++) {
            wstring p = ExpandPath(it->second.paths[j]);
            if (FileExists(p)) {
                return LaunchApp(p);
            }
        }
    }

    // ★ ② QQ 特判
    if (lower == L"qq") {
        const wchar_t* qqPaths[] = {
            L"C:\\Program Files\\Tencent\\QQNT\\QQ.exe",
            L"C:\\Program Files (x86)\\Tencent\\QQNT\\QQ.exe",
            L"C:\\Program Files\\Tencent\\QQ\\Bin\\QQ.exe",
            L"C:\\Program Files (x86)\\Tencent\\QQ\\Bin\\QQ.exe",
        };
        for (auto p : qqPaths) {
            if (FileExists(p)) {
                return LaunchApp(p);
            }
        }
    }

    // ③ 桌面快捷方式
    DesktopShortcut* sc = FindDesktopShortcut(cmd);
    if (sc) {
        if (OpenShortcut(*sc)) {
            Sleep(2000);
            return true;
        }
    }

    // ④ 注册表
    wstring exeName = cmd;
    if (exeName.find(L".exe") == wstring::npos) exeName += L".exe";
    wstring regPath = FindInRegistry(exeName);
    if (!regPath.empty() && FileExists(regPath)) {
        return LaunchApp(regPath);
    }

    return false;
}
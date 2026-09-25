#define _WIN32_WINNT 0x0600
#pragma execution_character_set("utf-8")

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iostream>
#include <sstream>
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>

#include "common.h"
#include "windows_util.h"
#include "input.h"
#include "apps.h"
#include "uia.h"
#include "system_ctrl.h"
#include "ai.h"
#include "vision.h"

#include "logger.h"
#include "webview_host.h"
#include "file_picker.h"
#include "notify.h"
#include "i18n.h"
#include "prompt.h"

extern std::vector<std::string> g_attachedPaths;

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")

using namespace std;

#define WM_TRAYICON     (WM_USER + 1)
#define WM_PICK_FILES   (WM_USER + 100)
#define WM_PICK_FOLDER  (WM_USER + 101)
#define WM_SEND_TO_JS   (WM_USER + 102)

const string CN_SEARCH = "\xE6\x90\x9C\xE7\xB4\xA2";
const string CN_INPUT = "\xE8\xBE\x93\xE5\x85\xA5";
const string CN_FILE = "\xE6\x96\x87\xE4\xBB\xB6";
const string CN_SAVE = "\xE4\xBF\x9D\xE5\xAD\x98";

const string KW_DOWNLOAD = "\xE4\xB8\x8B\xE8\xBD\xBD";
const string KW_SEARCH = "\xE6\x90\x9C\xE7\xB4\xA2";
const string KW_SEND = "\xE5\x8F\x91\xE9\x80\x81";
const string KW_THEN = "\xE7\x84\xB6\xE5\x90\x8E";
const string KW_AND = "\xE5\xB9\xB6\xE4\xB8\x94";

const string KW_NEW_FOLDER_1 = "\xE5\x88\x9B\xE5\xBB\xBA\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9";
const string KW_NEW_FOLDER_2 = "\xE6\x96\xB0\xE5\xBB\xBA\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9";

const string KW_NEW_TXT_1 = "\xE5\x88\x9B\xE5\xBB\xBA\xE6\x96\x87\xE6\x9C\xAC";
const string KW_NEW_TXT_2 = "\xE6\x96\xB0\xE5\xBB\xBA\xE6\x96\x87\xE6\x9C\xAC";
const string KW_NEW_TXT_3 = "\xE5\x88\x9B\xE5\xBB\xBA\xE6\x96\x87\xE4\xBB\xB6";
const string KW_NEW_TXT_4 = "\xE6\x96\xB0\xE5\xBB\xBA\xE6\x96\x87\xE4\xBB\xB6";

const string KW_NEW_BAT_1 = "\xE5\x88\x9B\xE5\xBB\xBA" "bat";
const string KW_NEW_BAT_2 = "\xE6\x96\xB0\xE5\xBB\xBA" "bat";

const string KW_CREATE = "\xE5\x88\x9B\xE5\xBB\xBA";
const string KW_NEW = "\xE6\x96\xB0\xE5\xBB\xBA";

const string KW_FILE_MGR_1 = "\xE6\x96\x87\xE4\xBB\xB6\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8";
const string KW_FILE_MGR_2 = "\xE8\xB5\x84\xE6\xBA\x90\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8";
const string KW_FILE_MGR_3 = "\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9";
const string KW_FILE_MGR_4 = "\xE7\x9B\xAE\xE5\xBD\x95";
const string KW_FILE_MGR_5 = "\xE6\x96\x87\xE4\xBB\xB6\xE8\xB5\x84\xE6\xBA\x90";

static std::atomic<bool> g_stopRequested{ false };
static std::atomic<bool> g_taskRunning{ false };
static std::atomic<bool> g_taskShouldEnd{ false };

bool InterruptibleSleep(int totalMs) {
    const int SLICE = 100;
    int elapsed = 0;
    while (elapsed < totalMs) {
        if (g_stopRequested) return false;
        int thisSlice = (totalMs - elapsed < SLICE) ? (totalMs - elapsed) : SLICE;
        Sleep(thisSlice);
        elapsed += thisSlice;
    }
    return true;
}

static std::wstring FindEdgePath() {
    const wchar_t* candidates[] = {
        L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
    };
    for (auto p : candidates) {
        if (FileExists(p)) return p;
    }
    return L"msedge.exe";
}

static void OpenUrlInEdge(const std::string& url) {
    std::string fullUrl = url;
    if (fullUrl.find("http://") != 0 && fullUrl.find("https://") != 0) {
        fullUrl = "https://" + fullUrl;
    }
    std::wstring edgePath = FindEdgePath();
    std::wstring wUrl = U2W(fullUrl);
    std::wstring args = L"--new-window \"" + wUrl + L"\"";
    LogExec("Opening URL in Edge: " + fullUrl);
    ShellExecuteW(nullptr, L"open", edgePath.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
}

struct UrlAlias { const char* kw; const char* url; };
static const UrlAlias g_urlAliases[] = {
    { "\xE4\xB8\x8B\xE8\xBD\xBDjava", "https://www.java.com/zh-CN/download/" },
    { "java\xE4\xB8\x8B\xE8\xBD\xBD", "https://www.java.com/zh-CN/download/" },
    { "java", "https://www.java.com/zh-CN/download/" },
    { "python", "https://www.python.org/downloads/" },
    { "git", "https://git-scm.com/downloads" },
    { "node", "https://nodejs.org/" },
    { "vscode", "https://code.visualstudio.com/" },
    { "chrome", "https://www.google.com/chrome/" },
};

static std::string ResolveUrlAlias(const std::string& task) {
    bool isDownload = (task.find(KW_DOWNLOAD) != std::string::npos);
    if (!isDownload) return "";
    for (auto& a : g_urlAliases) {
        if (task.find(a.kw) != std::string::npos) {
            return a.url;
        }
    }
    return "";
}

static std::string SanitizeFileName(const std::string& name) {
    std::string clean;
    for (char c : name) {
        if (c == '\\' || c == '/' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') continue;
        clean += c;
    }
    return clean;
}

static bool WriteFileContent(HANDLE hFile, const std::string& ext, const std::string& content) {
    if (content.empty()) return true;
    DWORD written = 0;

    if (ext == ".bat") {
        std::string toWrite = "@echo off\r\n" + content + "\r\n";
        int wlen = MultiByteToWideChar(CP_UTF8, 0, toWrite.c_str(), -1, nullptr, 0);
        std::wstring w(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, toWrite.c_str(), -1, &w[0], wlen);
        int alen = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string ansi(alen - 1, 0);
        WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &ansi[0], alen, nullptr, nullptr);
        return WriteFile(hFile, ansi.c_str(), (DWORD)ansi.size(), &written, nullptr) != 0;
    }
    else {
        const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        WriteFile(hFile, bom, 3, &written, nullptr);
        std::string toWrite = content + "\r\n";
        return WriteFile(hFile, toWrite.c_str(), (DWORD)toWrite.size(), &written, nullptr) != 0;
    }
}

static bool TryCreateFolder(const std::string& task) {
    std::string folderName;
    size_t pos = std::string::npos;

    if (task.find(KW_NEW_FOLDER_1) != std::string::npos) {
        pos = task.find(KW_NEW_FOLDER_1) + KW_NEW_FOLDER_1.size();
    }
    else if (task.find(KW_NEW_FOLDER_2) != std::string::npos) {
        pos = task.find(KW_NEW_FOLDER_2) + KW_NEW_FOLDER_2.size();
    }

    if (pos == std::string::npos || pos >= task.size()) return false;

    folderName = task.substr(pos);
    while (!folderName.empty() && folderName.front() == ' ') folderName.erase(0, 1);
    while (!folderName.empty() && folderName.back() == ' ') folderName.pop_back();
    if (folderName.empty()) return false;

    std::string cleanName = SanitizeFileName(folderName);
    if (cleanName.empty()) return false;

    wchar_t desktop[MAX_PATH] = { 0 };
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop))) {
        LogError("Cannot get desktop path");
        Notify(T("notify.title"), L"Failed to get desktop path");
        if (g_bridge.onDone) g_bridge.onDone(false, "create-folder-failed");
        return true;
    }

    std::wstring fullPath = std::wstring(desktop) + L"\\" + U2W(cleanName);

    if (CreateDirectoryW(fullPath.c_str(), nullptr)) {
        LogInfo("Created folder: " + W2U(fullPath));
        Notify(T("notify.title"), T("notify.taskDone"));
        if (g_bridge.onDone) g_bridge.onDone(true, "create-folder");
    }
    else {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_EXISTS) {
            LogInfo("Folder already exists (OK): " + cleanName);
            Notify(T("notify.title"), T("notify.taskDone"));
            if (g_bridge.onDone) g_bridge.onDone(true, "create-folder-exists");
        }
        else {
            LogError("Create folder failed: " + std::to_string(err));
            Notify(T("notify.title"), L"Create folder failed");
            if (g_bridge.onDone) g_bridge.onDone(false, "create-folder-failed");
        }
    }
    return true;
}

static bool TryCreateTextFile(const std::string& task) {
    bool isBat = (task.find("bat") != std::string::npos ||
        task.find("BAT") != std::string::npos ||
        task.find(KW_NEW_BAT_1) != std::string::npos ||
        task.find(KW_NEW_BAT_2) != std::string::npos);

    bool isTxt = (task.find("txt") != std::string::npos ||
        task.find("TXT") != std::string::npos ||
        task.find(KW_NEW_TXT_1) != std::string::npos ||
        task.find(KW_NEW_TXT_2) != std::string::npos ||
        task.find(KW_NEW_TXT_3) != std::string::npos ||
        task.find(KW_NEW_TXT_4) != std::string::npos);

    std::string ext;
    if (isBat) ext = ".bat";
    else if (isTxt) ext = ".txt";
    else return false;

    std::string rawName = task;
    const std::string prefixes[] = {
        KW_NEW_TXT_1, KW_NEW_TXT_2, KW_NEW_TXT_3, KW_NEW_TXT_4,
        KW_NEW_BAT_1, KW_NEW_BAT_2,
        KW_CREATE, KW_NEW,
    };
    for (auto& p : prefixes) {
        size_t pp = rawName.find(p);
        if (pp == 0) {
            rawName = rawName.substr(p.size());
            break;
        }
    }
    while (!rawName.empty() && rawName.front() == ' ') rawName.erase(0, 1);
    while (!rawName.empty() && rawName.back() == ' ') rawName.pop_back();

    const std::string descWords[] = {
        "\xE8\x87\xAA\xE5\x8A\xA8", "\xE4\xB8\x80\xE4\xB8\xAA", "\xE6\xBF\x80\xE6\xB4\xBB",
        "\xE7\x94\xA8\xE4\xBA\x8E", "\xE5\x8F\xAF\xE4\xBB\xA5", "\xE5\xAE\x9E\xE7\x8E\xB0",
        "\xE8\xAE\xA9", "\xE5\x81\x9A", "\xE8\x83\xBD", "\xE8\x87\xAA\xE5\xB7\xB1",
        "\xE5\xB8\xAE", "\xE7\x94\xA8\xE6\x9D\xA5", "\xE6\x89\x93\xE5\xBC\x80",
        "\xE5\x85\xB3\xE9\x97\xAD", "\xE4\xB8\x80\xE6\xAE\xB5",
    };
    for (auto& w : descWords) {
        if (rawName.find(w) != std::string::npos) {
            LogInfo("TryCreateTextFile: '" + rawName +
                "' looks like a description, deferring to AI");
            return false;
        }
    }

    if (rawName.empty()) {
        rawName = (ext == ".bat") ? "new_bat" : "new_file";
    }

    std::string fileNamePart;
    std::string contentPart;

    std::string lowerName = rawName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    size_t extPos = lowerName.find(ext);
    if (extPos != std::string::npos) {
        size_t nameEnd = extPos + ext.size();
        fileNamePart = rawName.substr(0, nameEnd);
        contentPart = rawName.substr(nameEnd);
        while (!contentPart.empty() && contentPart.front() == ' ') contentPart.erase(0, 1);
        while (!contentPart.empty() && contentPart.back() == ' ') contentPart.pop_back();
    }
    else {
        size_t sp = rawName.find(' ');
        if (sp == std::string::npos) {
            fileNamePart = rawName;
        }
        else {
            fileNamePart = rawName.substr(0, sp);
            contentPart = rawName.substr(sp + 1);
            while (!contentPart.empty() && contentPart.front() == ' ') contentPart.erase(0, 1);
            while (!contentPart.empty() && contentPart.back() == ' ') contentPart.pop_back();
        }
    }

    if (fileNamePart.size() > ext.size()) {
        std::string tail = fileNamePart.substr(fileNamePart.size() - ext.size());
        std::string lowerTail = tail;
        std::transform(lowerTail.begin(), lowerTail.end(), lowerTail.begin(), ::tolower);
        if (lowerTail == ext) {
            fileNamePart = fileNamePart.substr(0, fileNamePart.size() - ext.size());
        }
    }
    while (!fileNamePart.empty() && fileNamePart.front() == ' ') fileNamePart.erase(0, 1);
    while (!fileNamePart.empty() && fileNamePart.back() == ' ') fileNamePart.pop_back();

    std::string cleanName = SanitizeFileName(fileNamePart);
    if (cleanName.empty()) cleanName = (ext == ".bat") ? "new_bat" : "new_file";

    wchar_t desktop[MAX_PATH] = { 0 };
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop))) {
        LogError("Cannot get desktop path");
        Notify(T("notify.title"), L"Failed to get desktop path");
        if (g_bridge.onDone) g_bridge.onDone(false, "create-file-failed");
        return true;
    }

    std::wstring fullPath = std::wstring(desktop) + L"\\" + U2W(cleanName) + U2W(ext);

    HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        LogError("Create file failed: " + std::to_string(GetLastError()));
        Notify(T("notify.title"), L"Create file failed");
        if (g_bridge.onDone) g_bridge.onDone(false, "create-file-failed");
        return true;
    }

    WriteFileContent(hFile, ext, contentPart);
    CloseHandle(hFile);

    LogInfo("Created file: " + W2U(fullPath) +
        (contentPart.empty() ? "" : " with content"));
    Notify(T("notify.title"), T("notify.taskDone"));
    if (g_bridge.onDone) g_bridge.onDone(true, "create-file");
    return true;
}

string GetOpenWindowsList() {
    string result;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd)) return TRUE;
        wchar_t title[512];
        GetWindowTextW(hwnd, title, 512);
        if (wcslen(title) == 0) return TRUE;
        auto* s = reinterpret_cast<string*>(lParam);
        *s += "- " + W2U(title) + "\n";
        return TRUE;
        }, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool TryOpenDriveFromString(const string& s) {
    char driveChar = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            driveChar = toupper(c);
            break;
        }
    }
    if (driveChar < 'A' || driveChar > 'Z') return false;
    bool hasDriveWord = s.find("\xE7\x9B\x98") != string::npos;
    bool isDriveColon = (s.size() >= 2 && s[1] == ':');
    if (!hasDriveWord && !isDriveColon) return false;
    wstring drivePath;
    drivePath += (wchar_t)driveChar;
    drivePath += L":\\";
    if (GetDriveTypeW(drivePath.c_str()) == DRIVE_NO_ROOT_DIR) return false;
    LogExec("Opening drive " + std::string(1, (char)driveChar) + ":");
    ShellExecuteW(nullptr, L"open", drivePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    InterruptibleSleep(1500);
    return true;
}

void HandleSaveAsDialog() {
    InterruptibleSleep(1000);
    if (g_stopRequested) return;

    int x = 0, y = 0;
    if (FindInputByAI("\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D", x, y)) {
        if (x > 0) {
            SingleClickAt(x, y);
            InterruptibleSleep(300);
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('A', 0, 0, 0);
            keybd_event('A', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            InterruptibleSleep(200);
            TypeText(U2W("untitled"));
            InterruptibleSleep(300);
            PressKeyByName("enter");
            InterruptibleSleep(800);
            LogExec("Save-As dialog handled");
            return;
        }
    }
    PressKeyByName("enter");
    InterruptibleSleep(500);
    LogExec("Save-As: pressed Enter");
}

void ExecuteCommand(const string& cmd) {
    LogExec(cmd);

    if (cmd.find("CHAT:") == 0) {
        string answer = cmd.substr(5);
        while (!answer.empty() && answer.front() == ' ') answer.erase(0, 1);
        LogInfo(answer);
    }
    else if (cmd.find("RIGHTCLICK_DESKTOP") == 0) {
        LogExec("Right-click on desktop empty area");
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        RightClickAt(sw * 3 / 4, sh / 2);
        InterruptibleSleep(1500);
    }
    else if (cmd.find("CREATEFOLDER:") == 0) {
        std::string name = cmd.substr(13);
        while (!name.empty() && name.front() == ' ') name.erase(0, 1);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (name.empty()) { g_taskShouldEnd = true; return; }

        std::string cleanName = SanitizeFileName(name);
        if (cleanName.empty()) { g_taskShouldEnd = true; return; }

        wchar_t desktop[MAX_PATH] = { 0 };
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
        std::wstring fullPath = std::wstring(desktop) + L"\\" + U2W(cleanName);

        if (CreateDirectoryW(fullPath.c_str(), nullptr)) {
            LogInfo("Created folder: " + cleanName);
        }
        else {
            DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS) LogInfo("Folder already exists: " + cleanName);
            else LogError("Create folder failed: " + std::to_string(err));
        }
        g_taskShouldEnd = true;
        InterruptibleSleep(800);
    }
    else if (cmd.find("CREATEFILE:") == 0) {
        std::string rest = cmd.substr(11);
        while (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);

        const std::string SEP = ";;;";
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = rest.find(SEP);
        while (end != std::string::npos) {
            parts.push_back(rest.substr(start, end - start));
            start = end + SEP.size();
            end = rest.find(SEP, start);
        }
        parts.push_back(rest.substr(start));

        if (parts.size() < 2) { LogError("Invalid CREATEFILE format"); g_taskShouldEnd = true; return; }

        std::string ext = parts[0];
        std::string name = parts[1];
        std::string content = (parts.size() >= 3) ? parts[2] : "";

        auto trim = [](std::string& s) {
            while (!s.empty() && s.front() == ' ') s.erase(0, 1);
            while (!s.empty() && s.back() == ' ') s.pop_back();
            };
        trim(ext); trim(name); trim(content);

        std::string extDot;
        if (ext == "bat" || ext == "BAT") extDot = ".bat";
        else if (ext == "txt" || ext == "TXT") extDot = ".txt";
        else { LogError("CREATEFILE: unsupported ext"); g_taskShouldEnd = true; return; }

        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        if (lowerName.size() > extDot.size() &&
            lowerName.substr(lowerName.size() - extDot.size()) == extDot) {
            name = name.substr(0, name.size() - extDot.size());
        }

        std::string cleanName = SanitizeFileName(name);
        if (cleanName.empty()) cleanName = "new_file";

        wchar_t desktop[MAX_PATH] = { 0 };
        SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
        std::wstring fullPath = std::wstring(desktop) + L"\\" + U2W(cleanName) + U2W(extDot);

        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile == INVALID_HANDLE_VALUE) {
            LogError("Create file failed: " + std::to_string(GetLastError()));
            g_taskShouldEnd = true;
            InterruptibleSleep(500);
            return;
        }

        WriteFileContent(hFile, extDot, content);
        CloseHandle(hFile);
        LogInfo("Created file: " + cleanName + extDot);
        g_taskShouldEnd = true;
        InterruptibleSleep(800);
    }
    else if (cmd.find("OPEN:") == 0) {
        string prog = cmd.substr(5);
        while (!prog.empty() && prog.front() == ' ') prog.erase(0, 1);
        if (prog.empty()) return;

        std::string progLower = prog;
        for (auto& c : progLower) c = tolower(c);
        if (progLower == "qq" || progLower == "tencentqq") {
            const wchar_t* qqPaths[] = {
                L"C:\\Program Files\\Tencent\\QQNT\\QQ.exe",
                L"C:\\Program Files (x86)\\Tencent\\QQNT\\QQ.exe",
                L"C:\\Program Files\\Tencent\\QQ\\Bin\\QQ.exe",
                L"C:\\Program Files (x86)\\Tencent\\QQ\\Bin\\QQ.exe",
            };
            bool launched = false;
            for (auto p : qqPaths) {
                if (FileExists(p)) {
                    LogExec("Launching QQ: " + W2U(p));
                    ShellExecuteW(nullptr, L"open", p, nullptr, nullptr, SW_SHOWNORMAL);
                    InterruptibleSleep(3000);
                    launched = true;
                    break;
                }
            }
            if (!launched) {
                LogInfo("QQ.exe not found, opening download page");
                OpenUrlInEdge("https://im.qq.com/pcqq/index.shtml");
                InterruptibleSleep(3000);
            }
            return;
        }

        bool isUrl = false;
        if (prog.find("http://") == 0 || prog.find("https://") == 0 ||
            prog.find("www.") == 0) {
            isUrl = true;
        }

        if (isUrl) {
            OpenUrlInEdge(prog);
            InterruptibleSleep(3000);
            return;
        }

        wstring wProgCheck = U2W(prog);
        if (ActivateWindowByTitle(wProgCheck)) {
            LogInfo("Window already open, activated: " + prog);
            InterruptibleSleep(800);
            return;
        }

        if (TryOpenDriveFromString(prog)) return;
        wstring wProg = U2W(prog);
        if (TryBuiltinAction(prog, wProg)) return;
        if (TryOpenLocal(wProg)) return;
        vector<wstring> candidates = SearchExeCandidates(wProg);
        if (!candidates.empty()) {
            ShellExecuteW(nullptr, L"open", candidates[0].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            InterruptibleSleep(2000);
            return;
        }

        int x = 0, y = 0;
        if (FindDesktopIconByUIA(wProg, x, y)) {
            DoubleClickAt(x, y);
            InterruptibleSleep(1500);
        }
        else if (FindIconByAI(prog, x, y)) {
            DoubleClickAt(x, y);
            InterruptibleSleep(1500);
        }
    }
    else if (cmd.find("RIGHTCLICK:") == 0) {
        string what = cmd.substr(11);
        while (!what.empty() && what.front() == ' ') what.erase(0, 1);
        if (what.empty()) return;
        int x = 0, y = 0;
        if (FindDesktopIconByUIA(U2W(what), x, y)) { RightClickAt(x, y); InterruptibleSleep(1500); return; }
        if (FindClickableByUIA(U2W(what), x, y)) { RightClickAt(x, y); InterruptibleSleep(1500); return; }
        if (FindIconByAI(what, x, y)) { RightClickAt(x, y); InterruptibleSleep(1500); }
    }
    else if (cmd.find("CLICKMENU:") == 0) {
        string menu = cmd.substr(10);
        while (!menu.empty() && menu.front() == ' ') menu.erase(0, 1);
        if (menu.empty()) return;
        InterruptibleSleep(800);
        if (g_stopRequested) return;

        int x = 0, y = 0;
        if (FindMenuItemByUIA(U2W(menu), x, y)) { SingleClickAt(x, y); InterruptibleSleep(1500); return; }
        if (FindMenuTextByAI(menu, x, y)) { SingleClickAt(x, y); InterruptibleSleep(1500); }
    }
    else if (cmd.find("CLICK_INPUT:") == 0) {
        string hint = cmd.substr(12);
        while (!hint.empty() && hint.front() == ' ') hint.erase(0, 1);
        if (hint.empty()) return;
        string hintLower = hint;
        for (auto& c : hintLower) c = tolower(c);
        if (hintLower == "address" || hintLower == "url" || hintLower.find("address") != string::npos) {
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('L', 0, 0, 0);
            keybd_event('L', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            InterruptibleSleep(500);
            return;
        }
        int x = 0, y = 0;
        if (FindInputByAI(hint, x, y)) {
            if (x > 0) SingleClickAt(x, y);
            InterruptibleSleep(500);
        }
    }
    else if (cmd.find("CLICK:") == 0) {
        string what = cmd.substr(6);
        while (!what.empty() && what.front() == ' ') what.erase(0, 1);
        if (what.empty()) return;
        if (TryOpenDriveFromString(what)) return;
        if (what == CN_FILE || what == CN_SAVE) {
            LogExec("'" + what + "' -> Ctrl+S");
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('S', 0, 0, 0);
            keybd_event('S', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            InterruptibleSleep(1000);
            HandleSaveAsDialog();
            return;
        }
        if (what == CN_SEARCH) {
            LogExec("Ctrl+F (generic search)");
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('F', 0, 0, 0);
            keybd_event('F', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            InterruptibleSleep(800);
            return;
        }
        int x = 0, y = 0;
        if (FindDesktopIconByUIA(U2W(what), x, y)) { DoubleClickAt(x, y); InterruptibleSleep(1500); }
        else if (FindClickableByUIA(U2W(what), x, y)) { SingleClickAt(x, y); InterruptibleSleep(1500); }
        else if (FindIconByAI(what, x, y)) { DoubleClickAt(x, y); InterruptibleSleep(1500); }
    }
    else if (cmd.find("TYPE:") == 0) {
        string text = cmd.substr(5);
        while (!text.empty() && text.front() == ' ') text.erase(0, 1);
        TypeText(U2W(text));
        InterruptibleSleep(500);
    }
    else if (cmd.find("KEY:") == 0) {
        string key = cmd.substr(4);
        while (!key.empty() && key.front() == ' ') key.erase(0, 1);
        PressKeyByName(key);
        InterruptibleSleep(500);
    }
    else if (cmd.find("WAIT:") == 0) {
        string secStr = cmd.substr(5);
        while (!secStr.empty() && secStr.front() == ' ') secStr.erase(0, 1);
        try {
            int sec = stoi(secStr);
            for (int i = 0; i < sec * 10; i++) {
                if (g_stopRequested) return;
                Sleep(100);
            }
        }
        catch (...) {}
    }
}

void RunTask(const string& user_input) {
    LogInfo("RunTask called: " + user_input);

    if (g_taskRunning.exchange(true)) {
        LogError("A task is already running");
        return;
    }
    g_stopRequested = false;
    g_taskShouldEnd = false;

    LogInfo("Task: " + user_input);

    bool needFolder = (
        user_input.find(KW_FILE_MGR_1) != std::string::npos ||
        user_input.find(KW_FILE_MGR_2) != std::string::npos ||
        user_input.find(KW_FILE_MGR_3) != std::string::npos ||
        user_input.find(KW_FILE_MGR_4) != std::string::npos ||
        user_input.find(KW_FILE_MGR_5) != std::string::npos
        );

    bool isCreateFolder = (user_input.find(KW_NEW_FOLDER_1) != std::string::npos ||
        user_input.find(KW_NEW_FOLDER_2) != std::string::npos);

    if (needFolder && !isCreateFolder && g_attachedPaths.empty()) {
        LogInfo("Task mentions file manager — asking user to pick a folder...");
        Notify(T("notify.title"), L"Please select a folder");

        HWND hwnd = GetForegroundWindow();
        auto items = PickFolder(hwnd);

        if (!items.empty()) {
            for (auto& it : items) {
                g_attachedPaths.push_back(it.path);
                LogInfo("Attached: " + it.path);
            }
            NotifyPickedItems(items);
            InterruptibleSleep(500);
        }
        else {
            LogError("User cancelled folder selection");
            if (g_bridge.onDone) g_bridge.onDone(false, "cancelled");
            g_taskRunning = false;
            return;
        }
    }

    if (TryCreateFolder(user_input)) { g_taskRunning = false; return; }
    if (TryCreateTextFile(user_input)) { g_taskRunning = false; return; }

    std::string urlAlias = ResolveUrlAlias(user_input);
    if (!urlAlias.empty()) {
        LogInfo("Direct URL: " + urlAlias);
        OpenUrlInEdge(urlAlias);
        InterruptibleSleep(3000);
        Notify(T("notify.title"), T("notify.taskDone"));
        if (g_bridge.onDone) g_bridge.onDone(true, "url-direct");
        g_taskRunning = false;
        return;
    }

    bool shortInput = (user_input.size() <= 6);
    bool hasActionVerb = (user_input.find(KW_DOWNLOAD) != std::string::npos ||
        user_input.find(KW_SEARCH) != std::string::npos ||
        user_input.find(KW_SEND) != std::string::npos ||
        user_input.find(KW_THEN) != std::string::npos ||
        user_input.find(KW_AND) != std::string::npos);

    if (!IsMultiStep(user_input) && shortInput && !hasActionVerb) {
        if (TrySystemAdjust(user_input)) {
            Notify(T("notify.title"), T("notify.systemDone"));
            if (g_bridge.onDone) g_bridge.onDone(true, "system");
            g_taskRunning = false;
            return;
        }
        if (TryBuiltinAction(user_input, U2W(user_input))) {
            Notify(T("notify.title"), T("notify.taskDone"));
            if (g_bridge.onDone) g_bridge.onDone(true, "builtin");
            g_taskRunning = false;
            return;
        }
        if (TryOpenDrive(user_input)) {
            Notify(T("notify.title"), T("notify.opened"));
            if (g_bridge.onDone) g_bridge.onDone(true, "drive");
            g_taskRunning = false;
            return;
        }
    }

    string fullTask = user_input;
    int maxSteps = g_config.max_steps;
    if (maxSteps < 1 || maxSteps > 50) maxSteps = 12;
    bool taskDone = false;
    vector<string> taskHistory;

    for (int step = 0; step < maxSteps; step++) {
        if (g_stopRequested) { LogInfo("Stopped by user"); break; }

        if (g_bridge.onStep) g_bridge.onStep(step + 1, maxSteps);
        Log(LogLevel::Step, "Step " + to_string(step + 1));

        string imgB64 = CaptureScreenBase64();
        if (g_stopRequested) { LogInfo("Stopped after screenshot"); break; }

        PromptContext pctx;
        pctx.task = fullTask;
        pctx.step = step + 1;
        pctx.maxSteps = maxSteps;
        pctx.doneSteps = taskHistory;
        pctx.openWindows = GetOpenWindowsList();
        pctx.language = g_config.language;
        pctx.attachedPaths = g_attachedPaths;

        string prompt = BuildPrompt(pctx);
        if (g_stopRequested) { LogInfo("Stopped before AI call"); break; }

        string aiReply = CallAIVision(prompt, imgB64);
        if (g_stopRequested) { LogInfo("Stopped after AI call"); break; }

        string cmd = aiReply;
        size_t nl = cmd.find_first_of("\r\n");
        if (nl != string::npos) cmd = cmd.substr(0, nl);
        while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' ')) cmd.pop_back();
        while (!cmd.empty() && cmd.front() == ' ') cmd.erase(0, 1);

        if (cmd.find("ERROR:") == 0) {
            string errCode = cmd.substr(6);
            while (!errCode.empty() && errCode.front() == ' ') errCode.erase(0, 1);

            std::wstring friendlyMsgW;
            string errType = "unknown";
            if (errCode.find("AUTH_FAILED") != string::npos) { errType = "auth"; friendlyMsgW = T("err.auth"); }
            else if (errCode.find("INSUFFICIENT_QUOTA") != string::npos || errCode.find("quota") != string::npos || errCode.find("balance") != string::npos) { errType = "quota"; friendlyMsgW = T("err.quota"); }
            else if (errCode.find("RATE_LIMITED") != string::npos) { errType = "rate_limit"; friendlyMsgW = T("err.rateLimit"); }
            else if (errCode.find("PERMISSION_DENIED") != string::npos) { errType = "permission"; friendlyMsgW = T("err.permission"); }
            else if (errCode.find("SERVER_ERROR") != string::npos) { errType = "server"; friendlyMsgW = T("err.server"); }
            else if (errCode.find("NO_CHOICES") != string::npos || errCode.find("PARSE_FAILED") != string::npos || errCode.find("INVALID_JSON") != string::npos) { errType = "parse"; friendlyMsgW = T("err.parse"); }
            else if (errCode.find("WinHttp") != string::npos || errCode.find("EMPTY_RESPONSE") != string::npos) { errType = "network"; friendlyMsgW = T("err.network"); }
            else { std::wstring suffix(errCode.begin(), errCode.end()); friendlyMsgW = std::wstring(L"AI error: ") + suffix; }

            std::string friendlyMsg = W2U(friendlyMsgW);
            LogError(friendlyMsg);
            Notify(T("notify.title"), friendlyMsgW);

            if (g_bridge.onAIError) g_bridge.onAIError(errType, friendlyMsg);
            if (g_bridge.onDone) g_bridge.onDone(false, "ai-error");
            g_taskRunning = false;
            return;
        }

        LogAI(cmd);

        if (step == 0 && cmd.find("WAIT:") == 0) {
            LogError("AI tried WAIT on step 1, rejecting");
            taskHistory.push_back(cmd);
            InterruptibleSleep(500);
            continue;
        }

        if (cmd.find("CLICK:") == 0 && cmd.find("CLICK_INPUT:") != 0) {
            string name = cmd.substr(6);
            while (!name.empty() && name.front() == ' ') name.erase(0, 1);
            bool invalid = (name.size() > 20 || name.find('/') != string::npos ||
                name.find('.') != string::npos || name.find("http") != string::npos);
            if (invalid) {
                LogError("Invalid CLICK target: '" + name + "'");
                Notify(T("notify.title"), T("notify.invalidCmd"));
                if (g_bridge.onDone) g_bridge.onDone(false, "invalid-click");
                g_taskRunning = false;
                return;
            }
        }

        if (cmd == "DONE" || cmd == "done" || cmd.find("DONE") == 0) {
            LogInfo("TASK DONE");
            Notify(T("notify.title"), T("notify.taskDone"));
            taskDone = true;
            break;
        }

        if (cmd.find("CLICK_INPUT:") == 0) {
            string label = cmd.substr(12);
            while (!label.empty() && label.front() == ' ') label.erase(0, 1);
            bool valid = (label == "address" || label == "url" || label == "search" ||
                label == "\xE6\x90\x9C\xE7\xB4\xA2" ||
                label == "\xE8\xBE\x93\xE5\x85\xA5\xE6\xA1\x86");
            if (!valid || label.size() > 15 || label.find(' ') != string::npos) {
                LogError("Invalid CLICK_INPUT label: '" + label + "'");
                Notify(T("notify.title"), T("notify.invalidCmd"));
                if (g_bridge.onDone) g_bridge.onDone(false, "invalid-click-input");
                g_taskRunning = false;
                return;
            }
        }

        {
            const int LOOKBACK = 4;
            int sameCount = 0;
            int start = (int)taskHistory.size() - LOOKBACK;
            if (start < 0) start = 0;
            for (int i = start; i < (int)taskHistory.size(); i++) {
                if (taskHistory[i] == cmd) sameCount++;
            }
            if (sameCount >= 2) {
                LogError("Command '" + cmd + "' repeated, breaking loop");
                Notify(T("notify.title"), T("notify.loop"));
                if (g_bridge.onDone) g_bridge.onDone(false, "loop-detected");
                g_taskRunning = false;
                return;
            }

            if (cmd.find("WAIT:") == 0 && !taskHistory.empty()) {
                if (taskHistory.back().find("WAIT:") == 0) {
                    LogError("AI stuck in WAIT loop, breaking");
                    Notify(T("notify.title"), T("notify.stuck"));
                    if (g_bridge.onDone) g_bridge.onDone(false, "stuck-wait");
                    g_taskRunning = false;
                    return;
                }
            }
        }

        taskHistory.push_back(cmd);
        ExecuteCommand(cmd);

        if (g_taskShouldEnd) {
            LogInfo("Task done by CREATEFILE/CREATEFOLDER");
            taskDone = true;
            break;
        }

        if (g_stopRequested) { LogInfo("Stopped after command execution"); break; }
        InterruptibleSleep(1500);
    }

    if (g_stopRequested) {
        Notify(T("notify.title"), T("notify.taskStopped"));
        if (g_bridge.onDone) g_bridge.onDone(false, "stopped");
    }
    else if (taskDone) {
        if (g_bridge.onDone) g_bridge.onDone(true, "success");
    }
    else {
        Notify(T("notify.title"), T("notify.taskIncomplete"));
        if (g_bridge.onDone) g_bridge.onDone(false, "max-failed");
    }
    g_taskRunning = false;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        ResizeWebView(hwnd);
        break;
    case WM_NCCALCSIZE:
        return 0;
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);

        const int DRAG_H = 32;
        if (pt.y >= 0 && pt.y < DRAG_H) {
            if (pt.x > rc.right - 184) return HTCLIENT;
            return HTCAPTION;
        }

        const int B = 6;
        bool left = pt.x < B;
        bool right = pt.x > rc.right - B;
        bool top = pt.y < B;
        bool bottom = pt.y > rc.bottom - B;

        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left)            return HTLEFT;
        if (right)           return HTRIGHT;
        if (top)             return HTTOP;
        if (bottom)          return HTBOTTOM;
        return HTCLIENT;
    }
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;
    case WM_PICK_FILES: {
        auto items = PickFiles(hwnd, true);
        NotifyPickedItems(items);
        return 0;
    }
    case WM_PICK_FOLDER: {
        auto items = PickFolder(hwnd);
        NotifyPickedItems(items);
        return 0;
    }
    case WM_SEND_TO_JS:
        HandleSendToJS(lp);
        return 0;
    case WM_CLOSE:
        g_stopRequested = true;
        ShutdownNotify();
        ShutdownWebView2();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    InitUIAutomation();

    g_config = LoadConfig();

    if (g_config.base_url.empty()) {
        g_config.base_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    }
    if (g_config.model.empty()) {
        g_config.model = "qwen-vl-max";
    }
    if (g_config.max_steps < 1 || g_config.max_steps > 50) {
        g_config.max_steps = 12;
    }
    if (g_config.language != "en" && g_config.language != "zh") {
        g_config.language = "zh";
    }

    InitI18n(g_config.language);

    LogInfo("=== App starting ===");
    LogInfo("provider=" + g_config.provider + " model=" + g_config.model +
        " has_key=" + std::string(g_config.api_key.empty() ? "no" : "yes"));

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    const wchar_t* clsName = L"Project4AIWindow";
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = clsName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(clsName, L"AI Automation Assistant",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        LogError("CreateWindow failed");
        return 1;
    }

    int cornerPref = 2;
    DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));

    if (!InitNotify(hwnd)) {
        LogError("InitNotify failed");
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    std::wstring htmlPath = LR"(C:\Users\Administrator\Desktop\1\index.html)";
    LogInfo("htmlPath = " + W2U(htmlPath));

    SetTaskCallback([](const std::string& task) {
        LogInfo("TaskCallback fired: " + task);
        std::thread(RunTask, task).detach();
        });
    SetStopCallback([]() {
        LogInfo("StopCallback fired");
        g_stopRequested = true;
        });

    if (!InitWebView2(hwnd, htmlPath)) {
        LogError("WebView2 init failed");
        MessageBoxW(nullptr, L"WebView2 init failed. Please install WebView2 Runtime.",
            L"Error", MB_OK);
        return 1;
    }

    LogInfo("Entering message loop");

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_pAutomation) { g_pAutomation->Release(); g_pAutomation = nullptr; }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return 0;
}
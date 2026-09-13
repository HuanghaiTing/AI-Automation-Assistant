#define _WIN32_WINNT 0x0600
#pragma execution_character_set("utf-8")
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <sstream>
#include "common.h"
#include "windows_util.h"
#include "input.h"
#include "apps.h"
#include "uia.h"
#include "system_ctrl.h"
#include "ai.h"
#include "vision.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uiautomationcore.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace std;

const string CN_LOGIN = "\xE7\x99\xBB\xE5\xBD\x95";
const string CN_SEARCH = "\xE6\x90\x9C\xE7\xB4\xA2";
const string CN_INPUT = "\xE8\xBE\x93\xE5\x85\xA5";
const string CN_MY_PHONE = "\xE6\x88\x91\xE7\x9A\x84\xE6\x89\x8B\xE6\x9C\xBA";
const string CN_FILE = "\xE6\x96\x87\xE4\xBB\xB6";              // 文件
const string CN_SAVE = "\xE4\xBF\x9D\xE5\xAD\x98";              // 保存

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

string ScanSystemSoftware() {
    string result;
    wchar_t userDesktop[MAX_PATH] = { 0 };
    wchar_t commonDesktop[MAX_PATH] = { 0 };
    SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, userDesktop);
    SHGetFolderPathW(nullptr, CSIDL_COMMON_DESKTOPDIRECTORY, nullptr, 0, commonDesktop);
    wstring desktopDirs[] = { wstring(userDesktop), wstring(commonDesktop) };

    result += "[Desktop shortcuts]\n";
    for (int d = 0; d < 2; d++) {
        if (desktopDirs[d].empty()) continue;
        wstring pattern = desktopDirs[d] + L"\\*.lnk";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            wstring name = fd.cFileName;
            if (name.size() > 4) {
                wstring ext = name.substr(name.size() - 4);
                for (auto& c : ext) c = towlower(c);
                if (ext == L".lnk") name = name.substr(0, name.size() - 4);
            }
            result += "- " + W2U(name) + "\n";
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    const wchar_t* roots[] = {
        L"C:\\Program Files\\", L"C:\\Program Files (x86)\\",
        L"D:\\Program Files\\", L"D:\\Program Files (x86)\\",
        L"C:\\Users\\%USERNAME%\\AppData\\Local\\Programs\\",
        L"C:\\Users\\%USERNAME%\\AppData\\Local\\",
    };
    const int rootCount = sizeof(roots) / sizeof(roots[0]);

    result += "\n[Installed folders]\n";
    set<string> seen;
    for (int i = 0; i < rootCount; i++) {
        wstring root = ExpandPath(roots[i]);
        if (!FileExists(root)) continue;
        wstring pattern = root + L"*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            wstring name = fd.cFileName;
            if (name == L"." || name == L"..") continue;
            if (name == L"Windows" || name == L"$Recycle.Bin" || name == L"Common Files") continue;
            string utf8 = W2U(name);
            if (seen.find(utf8) == seen.end()) {
                seen.insert(utf8);
                result += "- " + utf8 + "\n";
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
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

    cout << "[EXEC] Opening drive " << (char)driveChar << ":" << endl;
    ShellExecuteW(nullptr, L"open", drivePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    Sleep(1500);
    return true;
}

// ★ 处理"另存为"对话框（记事本第一次保存会弹）
void HandleSaveAsDialog() {
    Sleep(1000);

    // 尝试找"文件名"输入框
    int x = 0, y = 0;
    if (FindInputByAI("\xE6\x96\x87\xE4\xBB\xB6\xE5\x90\x8D", x, y)) {
        if (x > 0) {
            SingleClickAt(x, y);
            Sleep(300);
            // 全选清空
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('A', 0, 0, 0);
            keybd_event('A', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            Sleep(200);
            TypeText(U2W("untitled"));
            Sleep(300);
            PressKeyByName("enter");
            Sleep(800);
            cout << "[EXEC] Save-As dialog handled" << endl;
            return;
        }
    }

    // 退化：直接按 Enter（默认焦点在"保存"按钮）
    PressKeyByName("enter");
    Sleep(500);
    cout << "[EXEC] Save-As: pressed Enter" << endl;
}

void ExecuteCommand(const string& cmd) {
    cout << "[CMD] " << cmd << endl;

    if (cmd.find("CHAT:") == 0) {
        string answer = cmd.substr(5);
        while (!answer.empty() && answer.front() == ' ') answer.erase(0, 1);
        cout << "\n" << answer << "\n" << endl;
    }
    else if (cmd.find("OPEN:") == 0) {
        string prog = cmd.substr(5);
        while (!prog.empty() && prog.front() == ' ') prog.erase(0, 1);
        if (prog.empty()) return;

        if (TryOpenDriveFromString(prog)) return;

        wstring wProg = U2W(prog);
        if (TryBuiltinAction(prog, wProg)) return;
        if (TryOpenLocal(wProg)) return;
        vector<wstring> candidates = SearchExeCandidates(wProg);
        if (!candidates.empty()) {
            ShellExecuteW(nullptr, L"open", candidates[0].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            Sleep(2000);
            return;
        }

        int x = 0, y = 0;
        if (FindDesktopIconByUIA(wProg, x, y)) {
            DoubleClickAt(x, y);
            Sleep(1500);
        }
        else if (FindIconByAI(prog, x, y)) {
            DoubleClickAt(x, y);
            Sleep(1500);
        }
    }
    else if (cmd.find("RIGHTCLICK:") == 0) {
        string what = cmd.substr(11);
        while (!what.empty() && what.front() == ' ') what.erase(0, 1);
        if (what.empty()) return;

        int x = 0, y = 0;
        if (FindClickableInWindow(L"QQ", U2W(what), x, y)) {
            RightClickAt(x, y);
            Sleep(1500);
            return;
        }
        if (FindDesktopIconByUIA(U2W(what), x, y)) {
            RightClickAt(x, y);
            Sleep(1500);
            return;
        }
        if (FindClickableByUIA(U2W(what), x, y)) {
            RightClickAt(x, y);
            Sleep(1500);
            return;
        }
        if (FindIconByAI(what, x, y)) {
            RightClickAt(x, y);
            Sleep(1500);
        }
    }
    else if (cmd.find("CLICKMENU:") == 0) {
        string menu = cmd.substr(10);
        while (!menu.empty() && menu.front() == ' ') menu.erase(0, 1);
        if (menu.empty()) return;
        Sleep(800);

        int x = 0, y = 0;
        if (FindMenuItemByUIA(U2W(menu), x, y)) {
            SingleClickAt(x, y);
            Sleep(1500);
            return;
        }
        if (FindMenuTextByAI(menu, x, y)) {
            SingleClickAt(x, y);
            Sleep(1500);
        }
    }
    else if (cmd.find("CLICK_INPUT:") == 0) {
        string hint = cmd.substr(12);
        while (!hint.empty() && hint.front() == ' ') hint.erase(0, 1);
        if (hint.empty()) return;

        wstring wHint = U2W(hint);
        if (FocusElementInWindow(L"QQ", wHint)) return;
        if (FocusQQInputBox()) return;

        string hintLower = hint;
        for (auto& c : hintLower) c = tolower(c);
        if (hintLower == "address" || hintLower == "url" || hintLower.find("address") != string::npos) {
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('L', 0, 0, 0);
            keybd_event('L', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            Sleep(500);
            return;
        }

        int x = 0, y = 0;
        if (FindInputByAI(hint, x, y)) {
            if (x > 0) SingleClickAt(x, y);
            Sleep(500);
        }
    }
    else if (cmd.find("CLICK:") == 0) {
        string what = cmd.substr(6);
        while (!what.empty() && what.front() == ' ') what.erase(0, 1);
        if (what.empty()) return;

        if (TryOpenDriveFromString(what)) return;

        // ★ "文件" / "保存" → Ctrl+S（记事本/浏览器/Office 通用）
        if (what == CN_FILE || what == CN_SAVE) {
            cout << "[EXEC] '" << what << "' -> using Ctrl+S" << endl;
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('S', 0, 0, 0);
            keybd_event('S', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            Sleep(1000);
            HandleSaveAsDialog();
            return;
        }

        // "搜索" → Ctrl+F
        if (what == CN_SEARCH) {
            cout << "[QQ] Using Ctrl+F" << endl;
            ActivateWindowByTitle(L"QQ");
            Sleep(300);
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('F', 0, 0, 0);
            keybd_event('F', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            Sleep(800);
            return;
        }

        // "我的手机" → Ctrl+F + 输入 + Enter
        if (what == CN_MY_PHONE) {
            cout << "[QQ] Searching 'My Phone'" << endl;
            ActivateWindowByTitle(L"QQ");
            Sleep(300);
            keybd_event(VK_CONTROL, 0, 0, 0);
            keybd_event('F', 0, 0, 0);
            keybd_event('F', 0, KEYEVENTF_KEYUP, 0);
            keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
            Sleep(800);
            TypeText(U2W(CN_MY_PHONE));
            Sleep(2000);
            PressKeyByName("enter");
            Sleep(1500);
            return;
        }

        // UIA Invoke
        if (ClickControlByName(U2W(what))) {
            Sleep(1500);
            return;
        }

        int x = 0, y = 0;
        if (FindDesktopIconByUIA(U2W(what), x, y)) {
            DoubleClickAt(x, y);
            Sleep(1500);
        }
        else if (FindClickableByUIA(U2W(what), x, y)) {
            SingleClickAt(x, y);
            Sleep(1500);
        }
        else if (FindIconByAI(what, x, y)) {
            DoubleClickAt(x, y);
            Sleep(1500);
        }
    }
    else if (cmd.find("TYPE:") == 0) {
        string text = cmd.substr(5);
        while (!text.empty() && text.front() == ' ') text.erase(0, 1);
        TypeText(U2W(text));
        Sleep(500);
    }
    else if (cmd.find("KEY:") == 0) {
        string key = cmd.substr(4);
        while (!key.empty() && key.front() == ' ') key.erase(0, 1);
        PressKeyByName(key);
        Sleep(500);
    }
    else if (cmd.find("WAIT:") == 0) {
        string secStr = cmd.substr(5);
        while (!secStr.empty() && secStr.front() == ' ') secStr.erase(0, 1);
        try { Sleep(stoi(secStr) * 1000); }
        catch (...) {}
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    InitUIAutomation();

    cout << "=== AI Automation Assistant ===" << endl;
    cout << "Scanning system..." << endl;
    string systemSoftware = ScanSystemSoftware();
    cout << "Done. Ready." << endl << endl;

    vector<string> history;
    string user_input;

    while (true) {
        cout << ">>> ";
        getline(cin, user_input);
        if (user_input == "exit" || user_input == "quit") break;
        if (user_input.empty()) continue;
        if (user_input == "dumpqq") { DumpQQElements(); continue; }

        if (!IsMultiStep(user_input)) {
            if (TrySystemAdjust(user_input)) continue;
            if (TryBuiltinAction(user_input, U2W(user_input))) continue;
            if (TryOpenDrive(user_input)) continue;
        }

        string fullTask = user_input;
        int maxSteps = 12;
        bool taskDone = false;
        vector<string> taskHistory;

        for (int step = 0; step < maxSteps; step++) {
            cout << "\n[Step " << (step + 1) << "]" << endl;

            string imgB64 = CaptureScreenBase64();

            vector<UIAControl> qqControls;
            GetQQControls(qqControls);

            string controlsList;
            for (size_t i = 0; i < qqControls.size() && i < 60; i++) {
                controlsList += "  - '" + W2U(qqControls[i].name) + "'\n";
            }

            string prompt;
            prompt += "You are controlling a Windows computer to complete a TASK.\n\n";
            prompt += "TASK: " + fullTask + "\n\n";

            if (!taskHistory.empty()) {
                prompt += "STEPS ALREADY DONE (DO NOT REPEAT THESE):\n";
                for (size_t i = 0; i < taskHistory.size(); i++) {
                    prompt += to_string(i + 1) + ". " + taskHistory[i] + "\n";
                }
                prompt += "\n";
            }

            prompt += "This is step " + to_string(step + 1) + ".\n";
            prompt += "Look at the CURRENT screenshot and the CONTROL LIST, then decide the NEXT SINGLE action.\n\n";

            prompt += "===========================================\n";
            prompt += "CURRENTLY OPEN WINDOWS:\n";
            prompt += GetOpenWindowsList() + "\n";
            prompt += "===========================================\n";

            if (!qqControls.empty()) {
                prompt += "QQ WINDOW CONTROLS (visible, clickable):\n";
                prompt += controlsList;
                prompt += "===========================================\n\n";
            }

            prompt += "Available COMMANDS:\n";
            prompt += "- OPEN: <program name>\n";
            prompt += "- CLICK: <control name from list>       (LEFT-click / double-click)\n";
            prompt += "- RIGHTCLICK: <control name>             (RIGHT-click to open context menu)\n";
            prompt += "- CLICKMENU: <menu item text>            (click an item in the right-click menu)\n";
            prompt += "- CLICK_INPUT: <input hint>\n";
            prompt += "- TYPE: <text>\n";
            prompt += "- KEY: <keyname>\n";
            prompt += "- WAIT: <seconds>\n";
            prompt += "- DONE\n\n";

            prompt += "CRITICAL RULES:\n";
            prompt += "- Output ONLY ONE command.\n";
            prompt += "- ★★ For RIGHT-CLICK menu (like Properties):\n";
            prompt += "     Step 1: RIGHTCLICK: <icon name>\n";
            prompt += "     Step 2: WAIT: 1\n";
            prompt += "     Step 3: CLICKMENU: <menu item like 属性>\n";
            prompt += "- ★★ To SAVE a file in ANY app (Notepad/Word/Browser), ALWAYS use: KEY: ctrl+s\n";
            prompt += "- ★★ NEVER use CLICK: 文件 or CLICK: 保存. They will fail.\n";
            prompt += "- ★★ When clicking a control in QQ, pick a name EXACTLY from the QQ WINDOW CONTROLS list.\n";
            prompt += "- ★★ Do NOT invent names.\n";
            prompt += "- If task complete: DONE\n";
            prompt += "- Do NOT repeat STEPS ALREADY DONE.\n\n";

            prompt += "EXAMPLES:\n";
            prompt += "- '右键此电脑点属性' -> RIGHTCLICK: 此电脑 / WAIT: 1 / CLICKMENU: 属性\n";
            prompt += "- '保存记事本' -> KEY: ctrl+s\n";
            prompt += "- '打开此电脑' -> CLICK: 此电脑\n\n";

            prompt += "QQ SPECIFIC:\n";
            prompt += "- QQ login screen: CLICK: ";
            prompt += CN_LOGIN;
            prompt += "\n";
            prompt += "- QQ search: CLICK: ";
            prompt += CN_SEARCH;
            prompt += "\n";
            prompt += "- ★ After CLICK: ";
            prompt += CN_SEARCH;
            prompt += ", NEXT step MUST be: TYPE: <contact name>\n";
            prompt += "- When chat window opens, TYPE: <message> then KEY: enter\n";
            prompt += "- When message sent: DONE\n\n";

            prompt += "Your reply (ONE command):";

            string aiReply = CallAIVision(prompt, imgB64);

            string cmd = aiReply;
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' '))
                cmd.pop_back();
            while (!cmd.empty() && cmd.front() == ' ')
                cmd.erase(0, 1);

            cout << "[AI] " << cmd << endl;

            if (cmd == "DONE" || cmd == "done" || cmd.find("DONE") == 0) {
                cout << "[TASK DONE]" << endl;
                taskDone = true;
                break;
            }

            if (taskHistory.size() >= 2) {
                string last1 = taskHistory[taskHistory.size() - 1];
                string last2 = taskHistory[taskHistory.size() - 2];
                if (cmd == last1 && cmd == last2) {
                    cout << "[WARN] Same command repeated 3x, breaking loop." << endl;
                    break;
                }
            }

            taskHistory.push_back(cmd);
            ExecuteCommand(cmd);
            Sleep(1500);
        }

        if (!taskDone) cout << "[WARN] Task reached max steps." << endl;
        history.push_back("user: " + user_input);
    }

    if (g_pAutomation) { g_pAutomation->Release(); g_pAutomation = nullptr; }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return 0;
}
#pragma once
#include "common.h"
#include <uiautomation.h>
#include <vector>
#include <string>

extern IUIAutomation* g_pAutomation;

// 控件信息结构
struct UIAControl {
    std::wstring name;       // 控件名
    int type;                // 控件类型
    int x;                   // 中心 X
    int y;                   // 中心 Y
};

bool InitUIAutomation();
bool GetQQControls(std::vector<UIAControl>& controls);   // ★ 新增
bool ClickControlByName(const std::wstring& name);        // ★ 新增（按名字点击）
bool ActivateWindowByTitle(const std::wstring& titlePart);
bool FocusQQInputBox();
void DumpQQElements();

bool FindDesktopIconByUIA(const std::wstring& label, int& outX, int& outY);
bool FindClickableByUIA(const std::wstring& label, int& outX, int& outY);
bool FindMenuItemByUIA(const std::wstring& label, int& outX, int& outY);
bool FocusInputByUIA(const std::wstring& hint);
bool FindClickableInWindow(const std::wstring& windowTitlePart, const std::wstring& label, int& outX, int& outY);
bool InvokeElementInWindow(const std::wstring& windowTitlePart, const std::wstring& label);
bool FocusElementInWindow(const std::wstring& windowTitlePart, const std::wstring& label);
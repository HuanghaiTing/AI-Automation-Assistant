#include "uia.h"
#include <iostream>

IUIAutomation* g_pAutomation = nullptr;

bool InitUIAutomation() {
    if (g_pAutomation) return true;
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation), (void**)&g_pAutomation);
    return SUCCEEDED(hr);
}

template<typename T>
void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

bool IsCoordOnScreen(int x, int y) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    return x >= 0 && x < sw && y >= 0 && y < sh;
}

void BringElementToFront(IUIAutomationElement* pElem) {
    if (!pElem) return;
    UIA_HWND uiaHwnd = 0;
    pElem->get_CurrentNativeWindowHandle(&uiaHwnd);
    HWND hwnd = (HWND)uiaHwnd;
    if (hwnd) {
        HWND top = GetAncestor(hwnd, GA_ROOT);
        if (top) hwnd = top;
    }
    if (!hwnd) return;
    ShowWindow(hwnd, SW_RESTORE);
    DWORD fgThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD curThread = GetCurrentThreadId();
    if (fgThread != curThread) {
        AttachThreadInput(fgThread, curThread, TRUE);
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
        AttachThreadInput(fgThread, curThread, FALSE);
    }
    else {
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
    }
    Sleep(300);
}

void BringDesktopToFront() {
    HWND hShell = GetShellWindow();
    if (hShell) {
        ShowWindow(hShell, SW_RESTORE);
        SetForegroundWindow(hShell);
        Sleep(200);
    }
}

void UncoverPoint(int x, int y) {
    POINT pt = { x, y };
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return;
    wchar_t className[256] = { 0 };
    GetClassNameW(hwnd, className, 256);
    std::wstring classNameStr(className);
    if (wcscmp(className, L"Progman") == 0 ||
        wcscmp(className, L"WorkerW") == 0 ||
        wcscmp(className, L"SHELLDLL_DefView") == 0 ||
        wcscmp(className, L"SysListView32") == 0 ||
        wcscmp(className, L"Shell_TrayWnd") == 0 ||
        wcscmp(className, L"TrayNotifyWnd") == 0) {
        return;
    }
    ShowWindow(hwnd, SW_MINIMIZE);
    Sleep(300);
    std::cout << "[UIA] Minimized blocking window: " << W2U(classNameStr) << std::endl;
}

bool ActivateWindowByTitle(const std::wstring& titlePart) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    if (!pArray) return false;
    int count = 0;
    pArray->get_Length(&count);

    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName == titlePart) {
            std::cout << "[UIA] Activating window (exact): " << W2U(elemName) << std::endl;
            BringElementToFront(pElem);
            SafeRelease(pElem);
            SafeRelease(pArray);
            return true;
        }
        SafeRelease(pElem);
    }

    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.find(titlePart) == 0) {
            std::cout << "[UIA] Activating window (prefix): " << W2U(elemName) << std::endl;
            BringElementToFront(pElem);
            SafeRelease(pElem);
            SafeRelease(pArray);
            return true;
        }
        SafeRelease(pElem);
    }

    SafeRelease(pArray);
    return false;
}

bool FindDesktopIconByUIA(const std::wstring& label, int& outX, int& outY) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Descendants, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    if (!pArray) return false;
    int count = 0;
    pArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    std::wstring bestName;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        if (type != UIA_ListItemControlTypeId) { SafeRelease(pElem); continue; }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }
        bool match = false;
        if (elemName == label) match = true;
        else if (elemName.find(label) != std::wstring::npos && elemName.size() <= label.size() + 4) match = true;
        else if (label.find(elemName) != std::wstring::npos && label.size() <= elemName.size() + 4) match = true;
        if (match) {
            pBest = pElem;
            bestName = elemName;
            break;
        }
        SafeRelease(pElem);
    }
    SafeRelease(pArray);
    if (!pBest) return false;
    RECT rect;
    pBest->get_CurrentBoundingRectangle(&rect);
    outX = (rect.left + rect.right) / 2;
    outY = (rect.top + rect.bottom) / 2;
    if (!IsCoordOnScreen(outX, outY)) {
        SafeRelease(pBest);
        return false;
    }
    BringDesktopToFront();
    UncoverPoint(outX, outY);
    std::cout << "[UIA-ICON] Found '" << W2U(bestName) << "' at (" << outX << ", " << outY << ")" << std::endl;
    SafeRelease(pBest);
    return true;
}

bool FindClickableByUIA(const std::wstring& label, int& outX, int& outY) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Descendants, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    if (!pArray) return false;
    int count = 0;
    pArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    std::wstring bestName;
    int bestScore = -1;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        bool clickable = false;
        switch (type) {
        case UIA_ButtonControlTypeId:
        case UIA_MenuItemControlTypeId:
        case UIA_ListItemControlTypeId:
        case UIA_TabItemControlTypeId:
        case UIA_HyperlinkControlTypeId:
        case UIA_TreeItemControlTypeId:
        case UIA_RadioButtonControlTypeId:
        case UIA_CheckBoxControlTypeId:
        case UIA_EditControlTypeId:
        case UIA_TextControlTypeId:
        case UIA_ImageControlTypeId:
            clickable = true;
            break;
        }
        if (!clickable) { SafeRelease(pElem); continue; }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }
        int score = -1;
        bool strictType = (type == UIA_TextControlTypeId || type == UIA_ImageControlTypeId);
        if (elemName == label) score = 100;
        else if (!strictType && elemName.find(label) != std::wstring::npos && elemName.size() <= label.size() + 4) score = 80;
        else if (!strictType && label.find(elemName) != std::wstring::npos && label.size() <= elemName.size() + 4) score = 60;
        else { SafeRelease(pElem); continue; }
        if (type == UIA_ButtonControlTypeId) score += 20;
        else if (type == UIA_MenuItemControlTypeId) score += 15;
        else if (type == UIA_TabItemControlTypeId) score += 10;
        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
            bestName = elemName;
        }
        SafeRelease(pElem);
    }
    SafeRelease(pArray);
    if (!pBest) return false;
    RECT rect;
    pBest->get_CurrentBoundingRectangle(&rect);
    outX = (rect.left + rect.right) / 2;
    outY = (rect.top + rect.bottom) / 2;
    if (!IsCoordOnScreen(outX, outY)) {
        SafeRelease(pBest);
        return false;
    }
    BringElementToFront(pBest);
    std::cout << "[UIA-CLICK] Found '" << W2U(bestName) << "' at (" << outX << ", " << outY << ")" << std::endl;
    SafeRelease(pBest);
    return true;
}

bool FindClickableInWindow(const std::wstring& windowTitlePart, const std::wstring& label, int& outX, int& outY) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    IUIAutomationElement* pTargetWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (!elemName.empty() &&
                (elemName == windowTitlePart || elemName.find(windowTitlePart) != std::wstring::npos)) {
                pTargetWindow = pElem;
                break;
            }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pTargetWindow) return false;
    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pTargetWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pTargetWindow);
    if (!pSubArray) return false;
    int count = 0;
    pSubArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    std::wstring bestName;
    int bestScore = -1;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        bool clickable = false;
        switch (type) {
        case UIA_ButtonControlTypeId:
        case UIA_MenuItemControlTypeId:
        case UIA_ListItemControlTypeId:
        case UIA_TabItemControlTypeId:
        case UIA_HyperlinkControlTypeId:
        case UIA_TreeItemControlTypeId:
        case UIA_RadioButtonControlTypeId:
        case UIA_CheckBoxControlTypeId:
        case UIA_EditControlTypeId:
        case UIA_TextControlTypeId:
        case UIA_ImageControlTypeId:
            clickable = true;
            break;
        }
        if (!clickable) { SafeRelease(pElem); continue; }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }
        int score = -1;
        bool strictType = (type == UIA_TextControlTypeId || type == UIA_ImageControlTypeId);
        if (elemName == label) score = 100;
        else if (!strictType && elemName.find(label) != std::wstring::npos && elemName.size() <= label.size() + 4) score = 80;
        else if (!strictType && label.find(elemName) != std::wstring::npos && label.size() <= elemName.size() + 4) score = 60;
        else { SafeRelease(pElem); continue; }
        if (type == UIA_ButtonControlTypeId) score += 20;
        else if (type == UIA_MenuItemControlTypeId) score += 15;
        else if (type == UIA_TabItemControlTypeId) score += 10;
        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
            bestName = elemName;
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
    if (!pBest) return false;
    RECT rect;
    pBest->get_CurrentBoundingRectangle(&rect);
    outX = (rect.left + rect.right) / 2;
    outY = (rect.top + rect.bottom) / 2;
    if (!IsCoordOnScreen(outX, outY)) {
        SafeRelease(pBest);
        return false;
    }
    BringElementToFront(pBest);
    std::cout << "[UIA-IN-WIN] Found '" << W2U(bestName) << "' at (" << outX << ", " << outY << ")" << std::endl;
    SafeRelease(pBest);
    return true;
}

bool FindMenuItemByUIA(const std::wstring& label, int& outX, int& outY) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Descendants, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    if (!pArray) return false;
    int count = 0;
    pArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    std::wstring bestName;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        if (type != UIA_MenuItemControlTypeId) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (!elemName.empty() && elemName.find(label) != std::wstring::npos) {
            pBest = pElem;
            bestName = elemName;
            break;
        }
        SafeRelease(pElem);
    }
    SafeRelease(pArray);
    if (!pBest) return false;
    RECT rect;
    pBest->get_CurrentBoundingRectangle(&rect);
    outX = (rect.left + rect.right) / 2;
    outY = (rect.top + rect.bottom) / 2;
    if (!IsCoordOnScreen(outX, outY)) {
        SafeRelease(pBest);
        return false;
    }
    BringElementToFront(pBest);
    SafeRelease(pBest);
    return true;
}

bool FocusInputByUIA(const std::wstring& hint) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Descendants, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    if (!pArray) return false;
    int count = 0;
    pArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    int bestScore = -1;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        if (type != UIA_EditControlTypeId && type != UIA_ComboBoxControlTypeId && type != UIA_DocumentControlTypeId) {
            SafeRelease(pElem); continue;
        }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        RECT rect;
        pElem->get_CurrentBoundingRectangle(&rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        int score = 0;
        if (!hint.empty() && elemName.find(hint) != std::wstring::npos) score += 100;
        if (type == UIA_EditControlTypeId) score += 40;
        if (width > 400) score += 30;
        if (height >= 15 && height <= 80) score += 20;
        if (rect.top < 200) score += 20;
        if (score > bestScore && score >= 50) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pElem->AddRef();
        }
        SafeRelease(pElem);
    }
    SafeRelease(pArray);
    if (!pBest) return false;
    BringElementToFront(pBest);
    HRESULT hr = pBest->SetFocus();
    SafeRelease(pBest);
    return SUCCEEDED(hr);
}

// ★ QQ 底部输入框
bool FocusQQInputBox() {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    IUIAutomationElement* pQQWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (elemName == L"QQ") { pQQWindow = pElem; break; }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pQQWindow) return false;
    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pQQWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pQQWindow);
    if (!pSubArray) return false;
    int count = 0;
    pSubArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    int bestScore = -1;
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        if (type != UIA_EditControlTypeId && type != UIA_DocumentControlTypeId) {
            SafeRelease(pElem); continue;
        }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        RECT rect;
        pElem->get_CurrentBoundingRectangle(&rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        int score = 0;
        if (rect.top > screenH / 2) score += 50;
        if (width > 300) score += 30;
        if (height > 30) score += 20;
        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
    if (!pBest || bestScore < 50) { SafeRelease(pBest); return false; }
    BringElementToFront(pBest);
    HRESULT hr = pBest->SetFocus();
    SafeRelease(pBest);
    if (SUCCEEDED(hr)) {
        std::cout << "[QQ] Input box focused" << std::endl;
        return true;
    }
    return false;
}

// ★ UIA Invoke（不点鼠标）
bool InvokeElementInWindow(const std::wstring& windowTitlePart, const std::wstring& label) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    IUIAutomationElement* pTargetWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (!elemName.empty() &&
                (elemName == windowTitlePart || elemName.find(windowTitlePart) != std::wstring::npos)) {
                pTargetWindow = pElem;
                break;
            }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pTargetWindow) return false;
    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pTargetWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pTargetWindow);
    if (!pSubArray) return false;
    int count = 0;
    pSubArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    int bestScore = -1;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        bool clickable = false;
        switch (type) {
        case UIA_ButtonControlTypeId:
        case UIA_MenuItemControlTypeId:
        case UIA_ListItemControlTypeId:
        case UIA_TabItemControlTypeId:
        case UIA_HyperlinkControlTypeId:
        case UIA_TreeItemControlTypeId:
        case UIA_RadioButtonControlTypeId:
        case UIA_CheckBoxControlTypeId:
        case UIA_TextControlTypeId:
        case UIA_ImageControlTypeId:
            clickable = true;
            break;
        }
        if (!clickable) { SafeRelease(pElem); continue; }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }
        int score = -1;
        bool strictType = (type == UIA_TextControlTypeId || type == UIA_ImageControlTypeId);
        if (elemName == label) score = 100;
        else if (!strictType && elemName.find(label) != std::wstring::npos && elemName.size() <= label.size() + 4) score = 80;
        else if (!strictType && label.find(elemName) != std::wstring::npos && label.size() <= elemName.size() + 4) score = 60;
        else { SafeRelease(pElem); continue; }
        if (type == UIA_ButtonControlTypeId) score += 20;
        else if (type == UIA_MenuItemControlTypeId) score += 15;
        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
    if (!pBest) return false;

    // Invoke
    IUIAutomationInvokePattern* pInvoke = nullptr;
    HRESULT hr = pBest->GetCurrentPatternAs(UIA_InvokePatternId,
        __uuidof(IUIAutomationInvokePattern),
        (void**)&pInvoke);
    if (SUCCEEDED(hr) && pInvoke) {
        BSTR bstrName = nullptr;
        pBest->get_CurrentName(&bstrName);
        std::wstring foundName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        std::cout << "[UIA-INVOKE] Invoking '" << W2U(foundName) << "'" << std::endl;
        pInvoke->Invoke();
        SafeRelease(pInvoke);
        SafeRelease(pBest);
        Sleep(500);
        return true;
    }

    // Selection
    IUIAutomationSelectionItemPattern* pSelect = nullptr;
    hr = pBest->GetCurrentPatternAs(UIA_SelectionItemPatternId,
        __uuidof(IUIAutomationSelectionItemPattern),
        (void**)&pSelect);
    if (SUCCEEDED(hr) && pSelect) {
        BSTR bstrName = nullptr;
        pBest->get_CurrentName(&bstrName);
        std::wstring foundName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        std::cout << "[UIA-SELECT] Selecting '" << W2U(foundName) << "'" << std::endl;
        pSelect->Select();
        SafeRelease(pSelect);
        SafeRelease(pBest);
        Sleep(500);
        return true;
    }

    SafeRelease(pBest);
    return false;
}

// ★ UIA SetFocus
bool FocusElementInWindow(const std::wstring& windowTitlePart, const std::wstring& label) {
    if (!InitUIAutomation()) return false;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    IUIAutomationElement* pTargetWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (!elemName.empty() &&
                (elemName == windowTitlePart || elemName.find(windowTitlePart) != std::wstring::npos)) {
                pTargetWindow = pElem;
                break;
            }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pTargetWindow) return false;
    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pTargetWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pTargetWindow);
    if (!pSubArray) return false;
    int count = 0;
    pSubArray->get_Length(&count);
    IUIAutomationElement* pBest = nullptr;
    int bestScore = -1;
    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        if (type != UIA_EditControlTypeId && type != UIA_DocumentControlTypeId) {
            SafeRelease(pElem);
            continue;
        }
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        int score = 50;
        if (!label.empty() && elemName.find(label) != std::wstring::npos) score += 50;
        RECT rect;
        pElem->get_CurrentBoundingRectangle(&rect);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        if (rect.top > screenH / 2) score += 20;
        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
    if (!pBest) return false;
    HRESULT hr = pBest->SetFocus();
    SafeRelease(pBest);
    if (SUCCEEDED(hr)) {
        std::cout << "[UIA-FOCUS] Focused element" << std::endl;
        Sleep(300);
        return true;
    }
    return false;
}

// ★ 调试
void DumpQQElements() {
    if (!InitUIAutomation()) return;
    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return;
    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);
    IUIAutomationElement* pQQWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        std::cout << "[DUMP] Top-level windows: " << count << std::endl;
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (!elemName.empty()) {
                std::cout << "[DUMP]   window[" << i << "]: " << W2U(elemName) << std::endl;
            }
            if (!elemName.empty() && elemName == L"QQ" && !pQQWindow) {
                pQQWindow = pElem;
                pElem->AddRef();
            }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pQQWindow) { std::cout << "[DUMP] QQ window NOT found" << std::endl; return; }
    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pQQWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pQQWindow);
    if (!pSubArray) return;
    int count = 0;
    pSubArray->get_Length(&count);
    std::cout << "[DUMP] Total " << count << " controls" << std::endl;
    for (int i = 0; i < count && i < 200; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;
        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }
        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        RECT rect;
        pElem->get_CurrentBoundingRectangle(&rect);
        if (!elemName.empty()) {
            std::cout << "[DUMP] [" << i << "] type=" << type
                << " name='" << W2U(elemName) << "'"
                << " at (" << rect.left << "," << rect.top << ")" << std::endl;
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
}

// ★ 扫描 QQ 窗口所有控件，返回列表
bool GetQQControls(std::vector<UIAControl>& controls) {
    if (!InitUIAutomation()) return false;

    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;

    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);

    IUIAutomationElement* pQQWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (elemName == L"QQ") { pQQWindow = pElem; break; }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pQQWindow) return false;

    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pQQWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pQQWindow);
    if (!pSubArray) return false;

    int count = 0;
    pSubArray->get_Length(&count);

    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;

        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);

        // 只要可点击的
        bool clickable = false;
        switch (type) {
        case UIA_ButtonControlTypeId:
        case UIA_MenuItemControlTypeId:
        case UIA_ListItemControlTypeId:
        case UIA_TabItemControlTypeId:
        case UIA_HyperlinkControlTypeId:
        case UIA_TreeItemControlTypeId:
        case UIA_RadioButtonControlTypeId:
        case UIA_CheckBoxControlTypeId:
        case UIA_TextControlTypeId:
        case UIA_ImageControlTypeId:
        case UIA_EditControlTypeId:
            clickable = true;
            break;
        }
        if (!clickable) { SafeRelease(pElem); continue; }

        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }

        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }

        RECT rect;
        pElem->get_CurrentBoundingRectangle(&rect);
        int cx = (rect.left + rect.right) / 2;
        int cy = (rect.top + rect.bottom) / 2;

        if (!IsCoordOnScreen(cx, cy)) { SafeRelease(pElem); continue; }

        UIAControl ctrl;
        ctrl.name = elemName;
        ctrl.type = (int)type;
        ctrl.x = cx;
        ctrl.y = cy;
        controls.push_back(ctrl);

        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);
    return true;
}

// ★ 按名字点击控件（在 QQ 窗口里找，UIA Invoke / 点击）
bool ClickControlByName(const std::wstring& name) {
    if (!InitUIAutomation()) return false;

    IUIAutomationElement* pRoot = nullptr;
    if (FAILED(g_pAutomation->GetRootElement(&pRoot))) return false;

    IUIAutomationCondition* pCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pCondition);
    IUIAutomationElementArray* pArray = nullptr;
    pRoot->FindAll(TreeScope_Children, pCondition, &pArray);
    SafeRelease(pCondition);
    SafeRelease(pRoot);

    IUIAutomationElement* pQQWindow = nullptr;
    if (pArray) {
        int count = 0;
        pArray->get_Length(&count);
        for (int i = 0; i < count; i++) {
            IUIAutomationElement* pElem = nullptr;
            if (FAILED(pArray->GetElement(i, &pElem))) continue;
            BSTR bstrName = nullptr;
            pElem->get_CurrentName(&bstrName);
            std::wstring elemName = bstrName ? bstrName : L"";
            if (bstrName) SysFreeString(bstrName);
            if (elemName == L"QQ") { pQQWindow = pElem; break; }
            SafeRelease(pElem);
        }
        SafeRelease(pArray);
    }
    if (!pQQWindow) return false;

    IUIAutomationCondition* pSubCondition = nullptr;
    g_pAutomation->CreateTrueCondition(&pSubCondition);
    IUIAutomationElementArray* pSubArray = nullptr;
    pQQWindow->FindAll(TreeScope_Descendants, pSubCondition, &pSubArray);
    SafeRelease(pSubCondition);
    SafeRelease(pQQWindow);
    if (!pSubArray) return false;

    int count = 0;
    pSubArray->get_Length(&count);

    IUIAutomationElement* pBest = nullptr;
    int bestScore = -1;

    for (int i = 0; i < count; i++) {
        IUIAutomationElement* pElem = nullptr;
        if (FAILED(pSubArray->GetElement(i, &pElem))) continue;

        CONTROLTYPEID type = 0;
        pElem->get_CurrentControlType(&type);
        bool clickable = false;
        switch (type) {
        case UIA_ButtonControlTypeId:
        case UIA_MenuItemControlTypeId:
        case UIA_ListItemControlTypeId:
        case UIA_TabItemControlTypeId:
        case UIA_HyperlinkControlTypeId:
        case UIA_TreeItemControlTypeId:
        case UIA_RadioButtonControlTypeId:
        case UIA_CheckBoxControlTypeId:
        case UIA_TextControlTypeId:
        case UIA_ImageControlTypeId:
            clickable = true;
            break;
        }
        if (!clickable) { SafeRelease(pElem); continue; }

        BOOL isOffscreen = FALSE;
        pElem->get_CurrentIsOffscreen(&isOffscreen);
        if (isOffscreen) { SafeRelease(pElem); continue; }

        BSTR bstrName = nullptr;
        pElem->get_CurrentName(&bstrName);
        std::wstring elemName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        if (elemName.empty()) { SafeRelease(pElem); continue; }

        int score = -1;
        bool strictType = (type == UIA_TextControlTypeId || type == UIA_ImageControlTypeId);
        if (elemName == name) score = 100;
        else if (!strictType && elemName.find(name) != std::wstring::npos && elemName.size() <= name.size() + 4) score = 80;
        else if (!strictType && name.find(elemName) != std::wstring::npos && name.size() <= elemName.size() + 4) score = 60;
        else { SafeRelease(pElem); continue; }

        if (type == UIA_ButtonControlTypeId) score += 20;
        else if (type == UIA_MenuItemControlTypeId) score += 15;

        if (score > bestScore) {
            bestScore = score;
            SafeRelease(pBest);
            pBest = pElem;
            pBest->AddRef();
        }
        SafeRelease(pElem);
    }
    SafeRelease(pSubArray);

    if (!pBest) return false;

    // ① Invoke
    IUIAutomationInvokePattern* pInvoke = nullptr;
    HRESULT hr = pBest->GetCurrentPatternAs(UIA_InvokePatternId,
        __uuidof(IUIAutomationInvokePattern),
        (void**)&pInvoke);
    if (SUCCEEDED(hr) && pInvoke) {
        BSTR bstrName = nullptr;
        pBest->get_CurrentName(&bstrName);
        std::wstring foundName = bstrName ? bstrName : L"";
        if (bstrName) SysFreeString(bstrName);
        std::cout << "[UIA-INVOKE] Invoking '" << W2U(foundName) << "'" << std::endl;
        pInvoke->Invoke();
        SafeRelease(pInvoke);
        SafeRelease(pBest);
        Sleep(500);
        return true;
    }

    // ② 退化：SetFocus + Enter
    RECT rect;
    pBest->get_CurrentBoundingRectangle(&rect);
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;
    SafeRelease(pBest);

    SetCursorPos(cx, cy);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(30);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(500);
    std::cout << "[UIA-CLICK] Clicked '" << W2U(name) << "' at (" << cx << ", " << cy << ")" << std::endl;
    return true;
}
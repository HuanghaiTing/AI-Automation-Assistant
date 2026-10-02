#define _WIN32_WINNT 0x0600
#include "office_automation.h"
#include "logger.h"
#include <windows.h>
#include <atlbase.h>
#include <comdef.h>
#include <iostream>

// ============ wstring -> UTF-8 string（用于日志） ============
static std::string W2ULocal(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

// ============ 多 ProgID 尝试 ============
static HRESULT FindFirstProgID(const wchar_t** progIds, int count, CLSID& clsid,
    std::wstring& foundProgId)
{
    for (int i = 0; i < count; i++) {
        HRESULT hr = CLSIDFromProgID(progIds[i], &clsid);
        if (SUCCEEDED(hr)) {
            foundProgId = progIds[i];
            LogInfo("[OFFICE] Found ProgID: " + W2ULocal(progIds[i]));
            return hr;
        }
    }
    return E_FAIL;
}

// ============ IDispatch 辅助函数 ============
// ★ 已修复：PROPERTYPUT 需要设置 DISPID_PROPERTYPUT 命名参数
static bool SetProperty(IDispatch* pDisp, LPCOLESTR name, VARIANT* value)
{
    if (!pDisp) return false;
    OLECHAR* names[] = { const_cast<OLECHAR*>(name) };
    DISPID dispID;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, names, 1,
        LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return false;

    // ★ 关键：设置命名参数 DISPID_PROPERTYPUT
    DISPID dispidPut = DISPID_PROPERTYPUT;
    DISPPARAMS params = { 0 };
    params.cArgs = 1;
    params.rgvarg = value;
    params.cNamedArgs = 1;
    params.rgdispidNamedArgs = &dispidPut;

    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYPUT,
        &params, nullptr, &excep, &argErr);

    // 如果失败，试试 PROPERTYPUTREF
    if (FAILED(hr)) {
        hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
            DISPATCH_PROPERTYPUTREF,
            &params, nullptr, &excep, &argErr);
    }
    return SUCCEEDED(hr);
}

static IDispatch* GetProperty(IDispatch* pDisp, LPCOLESTR name)
{
    if (!pDisp) return nullptr;
    OLECHAR* names[] = { const_cast<OLECHAR*>(name) };
    DISPID dispID;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, names, 1,
        LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return nullptr;

    DISPPARAMS params = { 0 };
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &params, &result, &excep, &argErr);
    if (SUCCEEDED(hr) && result.vt == VT_DISPATCH) {
        return result.pdispVal;
    }
    VariantClear(&result);
    return nullptr;
}

static IDispatch* InvokeMethodNoArgs(IDispatch* pDisp, LPCOLESTR name)
{
    if (!pDisp) return nullptr;
    OLECHAR* names[] = { const_cast<OLECHAR*>(name) };
    DISPID dispID;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, names, 1,
        LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return nullptr;

    DISPPARAMS params = { 0 };
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &params, &result, &excep, &argErr);
    if (SUCCEEDED(hr) && result.vt == VT_DISPATCH) {
        return result.pdispVal;
    }
    VariantClear(&result);
    return nullptr;
}

static bool InvokeMethodNoArgsVoid(IDispatch* pDisp, LPCOLESTR name)
{
    if (!pDisp) return false;
    OLECHAR* names[] = { const_cast<OLECHAR*>(name) };
    DISPID dispID;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, names, 1,
        LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return false;

    DISPPARAMS params = { 0 };
    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_METHOD,
        &params, nullptr, &excep, &argErr);
    return SUCCEEDED(hr);
}

static IDispatch* GetIndexedItem(IDispatch* pDisp, LPCOLESTR name, LONG index)
{
    if (!pDisp) return nullptr;
    OLECHAR* names[] = { const_cast<OLECHAR*>(name) };
    DISPID dispID;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, names, 1,
        LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return nullptr;

    VARIANT idx;
    VariantInit(&idx);
    idx.vt = VT_I4;
    idx.lVal = index;

    DISPPARAMS params = { &idx, nullptr, 1, 0 };
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    hr = pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET,
        &params, &result, &excep, &argErr);
    if (SUCCEEDED(hr) && result.vt == VT_DISPATCH) {
        return result.pdispVal;
    }
    VariantClear(&result);
    return nullptr;
}

// ============ 辅助：col 号 -> "A"、"B"... "AA"
static void ColToLetter(int col, wchar_t* buf, size_t bufSize)
{
    if (bufSize < 4) { buf[0] = 0; return; }
    if (col <= 26) {
        buf[0] = (wchar_t)(L'A' + col - 1);
        buf[1] = 0;
    }
    else {
        buf[0] = (wchar_t)(L'A' + (col - 1) / 26 - 1);
        buf[1] = (wchar_t)(L'A' + (col - 1) % 26);
        buf[2] = 0;
    }
}

// ============ 写一个单元格：Sheet.Cells(row, col).Value2 = text ============
static bool WriteCell(IDispatch* pSheet, int row1Based, int col1Based,
    const std::wstring& text)
{
    wchar_t colLetter[4] = { 0 };
    ColToLetter(col1Based, colLetter, 4);
    wchar_t addr[16] = { 0 };
    swprintf_s(addr, L"%s%d", colLetter, row1Based);

    // 1. Sheet.Cells -> IDispatch
    IDispatch* pCells = GetProperty(pSheet, L"Cells");
    if (!pCells) {
        LogError("[OFFICE] Sheet.Cells property failed");
        return false;
    }

    // 2. Cells(row, col) —— 默认成员，DISPID_VALUE
    //    COM 参数顺序：最后一个参数在 args[0]
    VARIANT args[2];
    VariantInit(&args[0]);
    VariantInit(&args[1]);
    args[0].vt = VT_I4;
    args[0].lVal = (LONG)col1Based;   // 第二个参数（列）
    args[1].vt = VT_I4;
    args[1].lVal = (LONG)row1Based;   // 第一个参数（行）

    DISPPARAMS getParams = { args, nullptr, 2, 0 };
    VARIANT result;
    VariantInit(&result);
    EXCEPINFO excep = { 0 };
    UINT argErr = 0;
    HRESULT hr = pCells->Invoke(DISPID_VALUE, IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET, &getParams, &result, &excep, &argErr);
    pCells->Release();

    if (FAILED(hr) || result.vt != VT_DISPATCH) {
        LogError("[OFFICE] Cells(" + std::to_string(row1Based) + "," +
            std::to_string(col1Based) + ") failed, hr=0x" +
            std::to_string(hr));
        VariantClear(&result);
        return false;
    }

    IDispatch* pCell = result.pdispVal;

    // 3. Cell.Value2 = text
    bool ok = false;
    HRESULT hrFinal = E_FAIL;
    {
        OLECHAR* names[] = { const_cast<OLECHAR*>(L"Value2") };
        DISPID dispID;
        HRESULT hrIds = pCell->GetIDsOfNames(IID_NULL, names, 1,
            LOCALE_USER_DEFAULT, &dispID);
        if (SUCCEEDED(hrIds)) {
            VARIANT val;
            VariantInit(&val);
            BSTR bstrVal = SysAllocString(text.c_str());
            val.vt = VT_BSTR;
            val.bstrVal = bstrVal;

            // ★ 关键：设置命名参数 DISPID_PROPERTYPUT
            DISPID dispidPut = DISPID_PROPERTYPUT;
            DISPPARAMS putParams = { 0 };
            putParams.cArgs = 1;
            putParams.rgvarg = &val;
            putParams.cNamedArgs = 1;
            putParams.rgdispidNamedArgs = &dispidPut;

            EXCEPINFO excep2 = { 0 };
            UINT argErr2 = 0;

            // 先试 PROPERTYPUT（最常见）
            HRESULT hr2 = pCell->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_PROPERTYPUT, &putParams, nullptr, &excep2, &argErr2);

            // 失败再试 PROPERTYPUTREF
            if (FAILED(hr2)) {
                hr2 = pCell->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                    DISPATCH_PROPERTYPUTREF, &putParams, nullptr, &excep2, &argErr2);
            }

            ok = SUCCEEDED(hr2);
            hrFinal = hr2;
            SysFreeString(bstrVal);
        }
        else {
            hrFinal = hrIds;
        }
    }
    pCell->Release();

    LogInfo("[OFFICE] cell " + W2ULocal(addr) + " = '" + W2ULocal(text) +
        "' ok=" + std::to_string(ok ? 1 : 0) +
        " hr=0x" + std::to_string(hrFinal));
    return ok;
}

// ============ Excel ============
int CreateExcelFile(const std::wstring& fullPath,
    const std::vector<std::vector<std::wstring>>& sheetData)
{
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrCo);

    // ★ 优先 Microsoft Office，失败则尝试 WPS 表格的各种 ProgID
    const wchar_t* excelProgIds[] = {
        L"Excel.Application",    // Microsoft Excel
        L"KET.Application",      // WPS 表格 (新版，msoAPI 兼容)
        L"et.Application",       // WPS 表格 (旧版，ksoAPI)
        L"ket.Application",
        L"ET.Application",
    };
    CLSID clsid;
    std::wstring foundProgId;
    HRESULT hr = FindFirstProgID(excelProgIds, 5, clsid, foundProgId);
    if (FAILED(hr)) {
        LogError("[OFFICE] Excel.Application / KET.Application not found. Is Excel or WPS installed?");
        if (needUninit) CoUninitialize();
        return -1;
    }

    IDispatch* pExcel = nullptr;
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
        IID_IDispatch, (void**)&pExcel);
    if (FAILED(hr) || !pExcel) {
        LogError("[OFFICE] Failed to create Excel instance, hr=0x" +
            std::to_string(hr));
        if (needUninit) CoUninitialize();
        return -2;
    }

    // Visible = false
    {
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_BOOL;
        v.boolVal = VARIANT_FALSE;
        SetProperty(pExcel, L"Visible", &v);
    }
    // DisplayAlerts = false
    {
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_BOOL;
        v.boolVal = VARIANT_FALSE;
        SetProperty(pExcel, L"DisplayAlerts", &v);
    }

    IDispatch* pWorkbooks = GetProperty(pExcel, L"Workbooks");
    if (!pWorkbooks) {
        LogError("[OFFICE] Excel.Workbooks not found");
        pExcel->Release();
        if (needUninit) CoUninitialize();
        return -3;
    }

    IDispatch* pWorkbook = InvokeMethodNoArgs(pWorkbooks, L"Add");
    pWorkbooks->Release();

    if (!pWorkbook) {
        LogError("[OFFICE] Excel.Workbooks.Add() failed");
        pExcel->Release();
        if (needUninit) CoUninitialize();
        return -4;
    }

    IDispatch* pSheet = nullptr;
    {
        IDispatch* pSheets = GetProperty(pWorkbook, L"Worksheets");
        if (pSheets) {
            pSheet = GetIndexedItem(pSheets, L"Item", 1);
            pSheets->Release();
        }
    }

    if (!pSheet) {
        LogError("[OFFICE] Excel.Worksheets(1) failed");
        InvokeMethodNoArgsVoid(pWorkbook, L"Close");
        pWorkbook->Release();
        InvokeMethodNoArgsVoid(pExcel, L"Quit");
        pExcel->Release();
        if (needUninit) CoUninitialize();
        return -5;
    }

    // 填数据
    for (size_t r = 0; r < sheetData.size(); r++) {
        for (size_t c = 0; c < sheetData[r].size(); c++) {
            WriteCell(pSheet, (int)(r + 1), (int)(c + 1), sheetData[r][c]);
        }
    }

    // SaveAs(fullPath)
    {
        OLECHAR* names[] = { const_cast<OLECHAR*>(L"SaveAs") };
        DISPID dispID;
        if (SUCCEEDED(pWorkbook->GetIDsOfNames(IID_NULL, names, 1,
            LOCALE_USER_DEFAULT, &dispID))) {
            VARIANT arg;
            VariantInit(&arg);
            BSTR bstr = SysAllocString(fullPath.c_str());
            arg.vt = VT_BSTR;
            arg.bstrVal = bstr;
            DISPPARAMS params = { &arg, nullptr, 1, 0 };
            EXCEPINFO excep = { 0 };
            UINT argErr = 0;
            HRESULT hr2 = pWorkbook->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_METHOD, &params, nullptr, &excep, &argErr);
            if (FAILED(hr2)) {
                LogError("[OFFICE] Excel SaveAs failed, hr=0x" +
                    std::to_string(hr2));
            }
            else {
                LogInfo("[OFFICE] Excel SaveAs OK");
            }
            SysFreeString(bstr);
        }
    }

    InvokeMethodNoArgsVoid(pWorkbook, L"Close");
    pSheet->Release();
    pWorkbook->Release();

    InvokeMethodNoArgsVoid(pExcel, L"Quit");
    pExcel->Release();

    if (needUninit) CoUninitialize();

    LogInfo("[OFFICE] Excel created OK via " + W2ULocal(foundProgId));
    return 0;
}

// ============ Word ============
int CreateWordFile(const std::wstring& fullPath,
    const std::vector<std::wstring>& paragraphs)
{
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrCo);

    const wchar_t* wordProgIds[] = {
        L"Word.Application",     // Microsoft Word
        L"KWPS.Application",     // WPS 文字 (新版，msoAPI 兼容)
        L"wps.Application",      // WPS 文字 (旧版/统一入口)
        L"kwps.Application",
        L"WPS.Application",
    };
    CLSID clsid;
    std::wstring foundProgId;
    HRESULT hr = FindFirstProgID(wordProgIds, 5, clsid, foundProgId);
    if (FAILED(hr)) {
        LogError("[OFFICE] Word.Application / KWPS.Application not found. Is Word or WPS installed?");
        if (needUninit) CoUninitialize();
        return -1;
    }

    IDispatch* pWord = nullptr;
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER,
        IID_IDispatch, (void**)&pWord);
    if (FAILED(hr) || !pWord) {
        LogError("[OFFICE] Failed to create Word instance, hr=0x" +
            std::to_string(hr));
        if (needUninit) CoUninitialize();
        return -2;
    }

    {
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_BOOL;
        v.boolVal = VARIANT_FALSE;
        SetProperty(pWord, L"Visible", &v);
    }
    {
        VARIANT v;
        VariantInit(&v);
        v.vt = VT_I4;
        v.lVal = 0;
        SetProperty(pWord, L"DisplayAlerts", &v);
    }

    IDispatch* pDocs = GetProperty(pWord, L"Documents");
    if (!pDocs) {
        LogError("[OFFICE] Word.Documents not found");
        pWord->Release();
        if (needUninit) CoUninitialize();
        return -3;
    }

    IDispatch* pDoc = InvokeMethodNoArgs(pDocs, L"Add");
    pDocs->Release();

    if (!pDoc) {
        LogError("[OFFICE] Word.Documents.Add() failed");
        InvokeMethodNoArgsVoid(pWord, L"Quit");
        pWord->Release();
        if (needUninit) CoUninitialize();
        return -4;
    }

    // 段落合并，\r 分隔
    {
        std::wstring allText;
        for (size_t i = 0; i < paragraphs.size(); i++) {
            allText += paragraphs[i];
            if (i + 1 < paragraphs.size()) {
                allText += L"\r";
            }
        }

        IDispatch* pRange = GetProperty(pDoc, L"Content");
        if (pRange) {
            BSTR bstr = SysAllocString(allText.c_str());
            VARIANT val;
            VariantInit(&val);
            val.vt = VT_BSTR;
            val.bstrVal = bstr;
            BOOL setOk = SetProperty(pRange, L"Text", &val);
            LogInfo("[OFFICE] Word Content.Text set ok=" +
                std::to_string(setOk ? 1 : 0));
            SysFreeString(bstr);
            pRange->Release();
        }
    }

    // SaveAs(fullPath)
    {
        OLECHAR* names[] = { const_cast<OLECHAR*>(L"SaveAs") };
        DISPID dispID;
        if (SUCCEEDED(pDoc->GetIDsOfNames(IID_NULL, names, 1,
            LOCALE_USER_DEFAULT, &dispID))) {
            VARIANT arg;
            VariantInit(&arg);
            BSTR bstr = SysAllocString(fullPath.c_str());
            arg.vt = VT_BSTR;
            arg.bstrVal = bstr;
            DISPPARAMS params = { &arg, nullptr, 1, 0 };
            EXCEPINFO excep = { 0 };
            UINT argErr = 0;
            HRESULT hr2 = pDoc->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                DISPATCH_METHOD, &params, nullptr, &excep, &argErr);
            if (FAILED(hr2)) {
                LogError("[OFFICE] Word SaveAs failed, hr=0x" +
                    std::to_string(hr2));
            }
            SysFreeString(bstr);
        }
    }

    InvokeMethodNoArgsVoid(pDoc, L"Close");
    pDoc->Release();

    InvokeMethodNoArgsVoid(pWord, L"Quit");
    pWord->Release();

    if (needUninit) CoUninitialize();

    LogInfo("[OFFICE] Word created OK via " + W2ULocal(foundProgId));
    return 0;
}
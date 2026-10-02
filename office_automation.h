#pragma once
#include <string>
#include <vector>

// 创建 / 覆盖 Excel 文件
// sheetData: 二维数组，第一行作为表头
// 返回：0 = 成功；-1 = 无 Excel；-2 = 创建实例失败；其它负数 = 各阶段失败
int CreateExcelFile(const std::wstring& fullPath,
    const std::vector<std::vector<std::wstring>>& sheetData);

// 创建 / 覆盖 Word 文件
// paragraphs: 段落列表
// 返回：0 = 成功；-1 = 无 Word；-2 = 创建实例失败；其它负数 = 各阶段失败
int CreateWordFile(const std::wstring& fullPath,
    const std::vector<std::wstring>& paragraphs);
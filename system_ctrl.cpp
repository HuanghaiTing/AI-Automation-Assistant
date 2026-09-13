#include "system_ctrl.h"
#include "common.h"
#include <shellapi.h>

// 最小实现以满足链接：如果输入像 X: 或 X:\ 则尝试以资源管理器打开对应驱动器/路径
bool TryOpenDrive(const std::string& input) {
	if (input.size() >= 2 && input[1] == ':') {
		std::wstring path = U2W(input);
		if (path.size() == 2) path += L"\\";
		HINSTANCE res = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		return ((INT_PTR)res) > 32;
	}
	return false;
}

// 系统设置快捷操作的最小实现：暂不处理，返回 false
bool TrySystemAdjust(const std::string& input) {
	(void)input;
	return false;
}

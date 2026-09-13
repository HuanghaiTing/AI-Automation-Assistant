#include "common.h"

const std::wstring API_HOST = L"dashscope.aliyuncs.com";
const std::wstring API_PATH = L"/compatible-mode/v1/chat/completions";
const std::string API_KEY = "sk-ws-H.PHMXIYR.WnUD.MEYCIQCo_JTKmDhrzvH5E6vJE569Z42X85hyx2987Riwx_tfZwIhAI458mCHr7tCe81MZvxdjNRe9GXRMCelyIJhtbbUlN7P";
const std::string MODEL_VISION = "qwen-vl-max";

std::string W2U(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring U2W(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string CleanUTF8(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { result += (char)c; i++; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) {
            result += s.substr(i, 2); i += 2;
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < s.size() &&
            ((unsigned char)s[i + 1] & 0xC0) == 0x80 && ((unsigned char)s[i + 2] & 0xC0) == 0x80) {
            result += s.substr(i, 3); i += 3;
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < s.size() &&
            ((unsigned char)s[i + 1] & 0xC0) == 0x80 && ((unsigned char)s[i + 2] & 0xC0) == 0x80 &&
            ((unsigned char)s[i + 3] & 0xC0) == 0x80) {
            result += s.substr(i, 4); i += 4;
        }
        else { i++; }
    }
    return result;
}

json SafeParseJSON(const std::string& raw) {
    // ★ 空字符串保护
    if (raw.empty()) return json();
    std::string cleaned = CleanUTF8(raw);
    if (cleaned.empty()) return json();
    return json::parse(cleaned, nullptr, false);
}

// ★ 修复：用 sizeof 自动算数组长度，不再硬编码
bool IsMultiStep(const std::string& input) {
    const char* separators[] = {
        "\xEF\xBC\x8C",              // ，
        "\xE7\x84\xB6\xE5\x90\x8E",  // 然后
        "\xE5\x86\x8D",              // 再
        "\xE8\xBE\x93\xE5\x85\xA5",  // 输入
        "\xE7\x82\xB9\xE5\x87\xBB",  // 点击
        "\xE5\xB9\xB6\xE4\xB8\x94",  // 并且
        "\xE5\x92\x8C",              // 和
        " and ",
        " & ",
    };
    const int count = sizeof(separators) / sizeof(separators[0]);   // ★ 自动算长度
    for (int i = 0; i < count; i++) {
        if (input.find(separators[i]) != std::string::npos) return true;
    }
    return false;
}
#include "i18n.h"
#include <windows.h>
#include <map>

static std::string g_lang = "zh";

// Translation table: [key][lang] = text
static std::map<std::string, std::map<std::string, std::wstring>> g_table = {
    // Task notifications
    { "notify.title",           {{"zh", L"AI \u52A9\u624B"},                       {"en", L"AI Assistant"}} },
    { "notify.taskDone",        {{"zh", L"\u2713 \u4EFB\u52A1\u5B8C\u6210"},       {"en", L"\u2713 Task completed"}} },
    { "notify.taskStopped",     {{"zh", L"\u25A0 \u4EFB\u52A1\u5DF2\u505C\u6B62"}, {"en", L"\u25A0 Task stopped"}} },
    { "notify.taskIncomplete",  {{"zh", L"\u2717 \u4EFB\u52A1\u672A\u5B8C\u6210"}, {"en", L"\u2717 Task incomplete"}} },
    { "notify.systemDone",      {{"zh", L"\u2713 \u7CFB\u7EDF\u64CD\u4F5C\u5B8C\u6210"}, {"en", L"\u2713 System action done"}} },
    { "notify.opened",          {{"zh", L"\u2713 \u6253\u5F00\u6210\u529F"},       {"en", L"\u2713 Opened"}} },
    { "notify.invalidCmd",      {{"zh", L"\u2717 AI \u547D\u4EE4\u65E0\u6548"},    {"en", L"\u2717 Invalid AI command"}} },
    { "notify.loop",            {{"zh", L"\u2717 AI \u9677\u5165\u5FAA\u73AF"},    {"en", L"\u2717 AI loop detected"}} },
    { "notify.stuck",           {{"zh", L"\u2717 AI \u5361\u5728\u7B49\u5F85"},    {"en", L"\u2717 AI stuck waiting"}} },

    // AI error details
    { "err.auth",               {{"zh", L"API Key \u65E0\u6548\u6216\u672A\u914D\u7F6E"},        {"en", L"API Key invalid or not configured"}} },
    { "err.quota",              {{"zh", L"Token \u989D\u5EA6\u4E0D\u8DB3\u6216\u6B20\u8D39"},   {"en", L"Token quota insufficient or account in arrears"}} },
    { "err.rateLimit",          {{"zh", L"\u8BF7\u6C42\u8FC7\u4E8E\u9891\u7E41"},                 {"en", L"Too many requests (rate limited)"}} },
    { "err.permission",         {{"zh", L"API Key \u6743\u9650\u4E0D\u8DB3"},                     {"en", L"API Key permission denied"}} },
    { "err.server",             {{"zh", L"AI \u670D\u52A1\u5668\u9519\u8BEF"},                   {"en", L"AI server error"}} },
    { "err.parse",              {{"zh", L"AI \u8FD4\u56DE\u683C\u5F0F\u5F02\u5E38"},             {"en", L"AI response format abnormal"}} },
    { "err.network",            {{"zh", L"\u7F51\u7EDC\u8FDE\u63A5\u5931\u8D25"},                 {"en", L"Network connection failed"}} },
};

void InitI18n(const std::string& lang) {
    if (lang == "en" || lang == "zh") {
        g_lang = lang;
    }
}

std::wstring T(const std::string& key) {
    auto it = g_table.find(key);
    if (it == g_table.end()) {
        return std::wstring(key.begin(), key.end());
    }
    auto lit = it->second.find(g_lang);
    if (lit == it->second.end()) {
        if (!it->second.empty()) return it->second.begin()->second;
        return std::wstring(key.begin(), key.end());
    }
    return lit->second;
}

std::string GetLanguage() {
    return g_lang;
}
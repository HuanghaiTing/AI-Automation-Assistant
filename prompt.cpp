#include "prompt.h"
#include <sstream>

// ========== Module 1: Task Context ==========
std::string ModuleTaskContext(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== TASK ===\n";
    ss << ctx.task << "\n";
    ss << "Step " << ctx.step << " / " << ctx.maxSteps << "\n";
    ss << "=== END TASK ===\n\n";

    if (!ctx.attachedPaths.empty()) {
        ss << "=== ATTACHED FILES/FOLDERS ===\n";
        ss << "User provided these paths. Use them directly (full path).\n";
        for (const auto& p : ctx.attachedPaths) {
            ss << "  " << p << "\n";
        }
        ss << "=== END ATTACHED ===\n\n";
    }

    ss << "=== PLAN (think first) ===\n";
    ss << "  - Open app      -> OPEN:<app>\n";
    ss << "  - Open website  -> OPEN:edge, then CLICK_INPUT:address, TYPE URL, KEY:enter\n";
    ss << "  - Create folder -> CREATEFOLDER:<name>\n";
    ss << "  - Create file   -> CREATEFILE:<ext>;;;<name>;;;<content>\n";
    ss << "  - Interact UI   -> CLICK the right element\n";
    ss << "  - Done          -> DONE\n";
    ss << "Start with a REAL action. Do NOT WAIT on step 1.\n";
    ss << "=== END PLAN ===\n\n";
    return ss.str();
}

// ========== Module 2: State Snapshot ==========
std::string ModuleStateSnapshot(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== CURRENT STATE ===\n";

    if (!ctx.doneSteps.empty()) {
        ss << "[Already Done]:\n";
        for (const auto& s : ctx.doneSteps) ss << "  - " << s << "\n";
    }
    else {
        ss << "[Already Done]: (none)\n";
    }

    ss << "\n[Open Windows]:\n";
    if (ctx.openWindows.empty()) ss << "  (none)\n";
    else ss << ctx.openWindows;

    ss << "=== END STATE ===\n\n";
    return ss.str();
}

// ========== Module 3: Command Contract ==========
std::string ModuleCommandContract(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== COMMAND FORMAT ===\n";
    ss << "Reply EXACTLY ONE command on ONE line.\n\n";
    ss << "Available commands:\n";
    ss << "  OPEN:<app|url|path>          - Launch app / open URL / open file\n";
    ss << "  CLICK:<name>                 - Left click element (SHORT name)\n";
    ss << "  RIGHTCLICK:<name>            - Right click element\n";
    ss << "  RIGHTCLICK_DESKTOP           - Right-click desktop empty area\n";
    ss << "  CLICKMENU:<item>             - Click a menu item\n";
    ss << "  CLICK_INPUT:<label>          - Focus a text input box\n";
    ss << "  TYPE:<text>                  - Type text into focused input\n";
    ss << "  KEY:<key>                    - Press a key (win+r, ctrl+s, enter)\n";
    ss << "  WAIT:<sec>                   - Wait N seconds\n";
    ss << "  CREATEFOLDER:<name>          - Create folder on Desktop\n";
    ss << "  CREATEFILE:<ext>;;;<name>;;;<content>  - Create file on Desktop\n";
    ss << "       ext: txt or bat\n";
    ss << "       separator: ';;;' (three semicolons)\n";
    ss << "       content MAY contain '|' but NOT ';;;'\n";
    ss << "       ★ Content MUST contain the ACTUAL command (for bat).\n";
    ss << "       ★ Do NOT start with '@echo off' — the system adds it.\n";
    ss << "       Example: CREATEFILE:txt;;;note;;;hello world\n";
    ss << "       Example: CREATEFILE:bat;;;auto_shutdown;;;shutdown /s /t 60\n";
    ss << "  DONE                         - Task complete\n\n";
    ss << "CLICK_INPUT valid labels ONLY:\n";
    ss << "  address | url | search | \xE6\x90\x9C\xE7\xB4\xA2 | \xE8\xBE\x93\xE5\x85\xA5\xE6\xA1\x86\n";
    ss << "=== END COMMAND FORMAT ===\n\n";
    return ss.str();
}

// ========== Module 4: Hard Rules ==========
std::string ModuleHardRules(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== HARD RULES ===\n";
    ss << "0. WAIT only for slow loading, NEVER first action.\n";
    ss << "1. ONE command, ONE line, no extra text.\n";
    ss << "2. Don't repeat [Already Done].\n";
    ss << "3. Don't OPEN app already in [Open Windows].\n";
    ss << "4. NEVER WAIT twice in a row.\n";
    ss << "5. On STEP 1, do NOT WAIT. Start with OPEN / CLICK / CREATEFILE.\n";
    ss << "6. CLICK_INPUT takes ONLY a valid label (see list).\n";
    ss << "7. CLICK target must be SHORT (<20 chars). Never URL or page title.\n";
    ss << "8. For file ops, use FULL PATH from [ATTACHED FILES/FOLDERS].\n";
    ss << "9. If screenshot shows task DONE, reply DONE.\n";
    ss << "10. CREATEFILE uses ';;;' as separator (NOT '|').\n";
    ss << "11. After CREATEFILE / CREATEFOLDER succeeds, reply DONE next step.\n";
    ss << "12. For BAT files, content MUST be a REAL command (e.g. 'shutdown /s /t 60').\n";
    ss << "13. NEVER write '@echo off' as content — the system adds it automatically.\n";
    ss << "=== END HARD RULES ===\n\n";
    return ss.str();
}

// ========== Module 5: App Hints ==========
std::string ModuleAppHints(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== APP-SPECIFIC HINTS ===\n";

    ss << "[Browser]:\n";
    ss << "  - ALWAYS use Edge (OPEN:edge), NEVER Chrome.\n";

    ss << "[Open website]:\n";
    ss << "  - Chain: OPEN:edge -> CLICK_INPUT:address -> TYPE:<url> -> KEY:enter\n";
    ss << "  - Or: OPEN:https://www.java.com/zh-CN/download/\n";

    ss << "[System info (IP/network)]:\n";
    ss << "  - KEY:win+r | TYPE:cmd /k ipconfig | KEY:enter\n";

    ss << "[Windows settings]:\n";
    ss << "  - OPEN:settings\n";

    ss << "[Open QQ]:\n";
    ss << "  - OPEN:qq  (system will find and launch it)\n";

    ss << "[Create folder/file]:\n";
    ss << "  - Folder: CREATEFOLDER:<name>\n";
    ss << "  - File:   CREATEFILE:<ext>;;;<name>;;;<content>\n";
    ss << "  - Never put ';;;' inside the content.\n";

    ss << "[BAT scripts]:\n";
    ss << "  - Content MUST include the ACTUAL command.\n";
    ss << "  - Do NOT start with '@echo off' (added by system).\n";
    ss << "  - Examples:\n";
    ss << "    - Shutdown: CREATEFILE:bat;;;auto_shutdown;;;shutdown /s /t 60\n";
    ss << "    - Restart:  CREATEFILE:bat;;;restart;;;shutdown /r /t 0\n";
    ss << "    - Open calc: CREATEFILE:bat;;;open_calc;;;start calc\n";
    ss << "    - Activate: CREATEFILE:bat;;;activate;;;slmgr.vbs -ato\n";

    ss << "[Save any file]:\n";
    ss << "  - KEY:ctrl+s\n";

    ss << "=== END APP HINTS ===\n\n";
    return ss.str();
}

// ========== Module 6: Reference ==========
std::string ModuleReferenceExamples(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== REFERENCE (do NOT copy) ===\n";
    ss << "  'view IP'           -> KEY:win+r -> TYPE:ipconfig -> KEY:enter\n";
    ss << "  'download java'     -> OPEN:edge -> CLICK_INPUT:address -> TYPE:java.com/download -> KEY:enter\n";
    ss << "  'open QQ'           -> OPEN:qq\n";
    ss << "  'make folder abc'   -> CREATEFOLDER:abc\n";
    ss << "  'make txt note'     -> CREATEFILE:txt;;;note;;;hello world\n";
    ss << "  'auto shutdown bat' -> CREATEFILE:bat;;;auto_shutdown;;;shutdown /s /t 60\n";
    ss << "=== END REFERENCE ===\n\n";
    return ss.str();
}

// ========== Build Full Prompt ==========
std::string BuildPrompt(const PromptContext& ctx) {
    std::string prompt;
    prompt += ModuleTaskContext(ctx);
    prompt += ModuleStateSnapshot(ctx);
    prompt += ModuleCommandContract(ctx);
    prompt += ModuleHardRules(ctx);
    prompt += ModuleAppHints(ctx);
    prompt += ModuleReferenceExamples(ctx);
    prompt += "Your ONE command:";
    return prompt;
}
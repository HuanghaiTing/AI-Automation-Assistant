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
    ss << "  - Create Excel  -> CREATEEXCEL:<name>;;;<row1>;;;<row2>...\n";
    ss << "  - Create Word   -> CREATEWORD:<name>;;;<p1>;;;<p2>...\n";
    ss << "  - Modify file   -> EDITFILE / EDITEXCEL / EDITWORD (with FULL PATH)\n";
    ss << "  - Append to file -> APPENDFILE / APPENDEXCEL / APPENDWORD\n";
    ss << "  - Open existing -> OPENFILE:<path>\n";
    ss << "  - Read file     -> READFILE:<path>\n";
    ss << "  - List files    -> LISTFILE:<ext>\n";
    ss << "  - Done          -> DONE\n";
    ss << "Start with a REAL action. Do NOT WAIT on step 1.\n";
    ss << "If the task is a SINGLE simple action, reply DONE right after it succeeds.\n";
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

    ss << "\n[Desktop Files]:\n";
    if (ctx.desktopFiles.empty()) ss << "  (none)\n";
    else ss << ctx.desktopFiles;

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
    ss << "\n";
    ss << "  CREATEFILE:<ext>;;;<name>;;;<content>\n";
    ss << "       ext: txt or bat\n";
    ss << "       separator: ';;;'\n";
    ss << "       YOU MUST WRITE the content yourself if user asked for text.\n";
    ss << "       Example: user says 'write 200-char note about cats'\n";
    ss << "         -> CREATEFILE:txt;;;cat_note;;;Cats are wonderful pets...\n";
    ss << "         NOT: CREATEFILE:txt;;;cat_note;;;write 200-char note   <-- WRONG\n";
    ss << "\n";
    ss << "  CREATEEXCEL:<name>;;;<row1>;;;<row2>;;;...\n";
    ss << "       Cells in a row: separated by '|'\n";
    ss << "       First row = header\n";
    ss << "       YOU MUST FILL the data yourself if user described it.\n";
    ss << "       Example: 'make excel with 3 students and grades'\n";
    ss << "         -> CREATEEXCEL:grades;;;Name|Math|English;;;Alice|90|85;;;Bob|88|92;;;Carol|75|80\n";
    ss << "\n";
    ss << "  CREATEWORD:<name>;;;<p1>;;;<p2>;;;...\n";
    ss << "       separator between paragraphs: ';;;'\n";
    ss << "       *** YOU ARE THE WRITER. Generate the ACTUAL Chinese text yourself. ***\n";
    ss << "       The user gives the TOPIC; you produce the CONTENT.\n";
    ss << "       Example: user says 'write 300-char essay on growth'\n";
    ss << "         -> CREATEWORD:growth_essay;;;成长是一场漫长而美丽的旅程。每个人都会经历成长，但方式各不相同。\n";
    ss << "         NOT: CREATEWORD:growth_essay;;;write 300-char essay   <-- WRONG!\n";
    ss << "\n";
    ss << "  LISTFILE:<ext>               - List files on Desktop (e.g. LISTFILE:txt)\n";
    ss << "  READFILE:<path>              - Read text file content (full path)\n";
    ss << "  OPENFILE:<path>              - Open existing file with default app\n";
    ss << "  EDITFILE:<path>;;;<content>  - Overwrite txt/bat file content\n";
    ss << "  EDITEXCEL:<path>;;;<row1>;;;<row2>;;;...  - Overwrite Excel content\n";
    ss << "  EDITWORD:<path>;;;<p1>;;;<p2>;;;...  - Overwrite Word content\n";
    ss << "  APPENDFILE:<path>;;;<content>  - Append content to txt/bat\n";
    ss << "  APPENDEXCEL:<path>;;;<row>      - Append a row to Excel\n";
    ss << "  APPENDWORD:<path>;;;<paragraph> - Append a paragraph to Word\n";
    ss << "  CHAT:<text>                  - Reply to the user with TEXT (for questions)\n";
    ss << "  DONE                         - Task complete\n\n";
    ss << "CLICK_INPUT valid labels ONLY:\n";
    ss << "  address | url | search | 搜索 | 输入框\n";
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
    ss << "8. For file ops, use FULL PATH from [ATTACHED FILES/FOLDERS] or [Desktop Files].\n";
    ss << "9. If screenshot shows task DONE, reply DONE.\n";
    ss << "10. CREATEFILE uses ';;;' as separator (NOT '|').\n";
    ss << "11. After CREATEFILE / CREATEFOLDER succeeds, reply DONE next step.\n";
    ss << "12. For BAT files, content MUST be a REAL command (e.g. 'shutdown /s /t 60').\n";
    ss << "13. NEVER write '@echo off' as content - the system adds it automatically.\n";
    ss << "14. SINGLE-ACTION task: after the action succeeds, reply DONE immediately.\n";
    ss << "15. Do NOT add steps the user didn't ask for.\n";
    ss << "16. '打开浏览器' / 'open browser' means ONLY open it.\n";
    ss << "17. '打开设置' / 'open settings' means ONLY open it.\n";
    ss << "18. '打开' + app name = OPEN: only.\n";
    ss << "19. Excel -> CREATEEXCEL / EDITEXCEL / APPENDEXCEL. NEVER CREATEFILE.\n";
    ss << "20. Word -> CREATEWORD / EDITWORD / APPENDWORD. NEVER CREATEFILE.\n";
    ss << "21. Do NOT include '.xlsx' / '.docx' in the name for CREATE commands.\n";
    ss << "22. Both go to Desktop, just like CREATEFILE.\n";
    ss << "23. If user asks a QUESTION, reply with 'CHAT:<answer>'.\n";
    ss << "24. CHAT: is for questions only. Do NOT mix with commands.\n";
    ss << "25. When user says '修改' / '编辑' / 'modify' existing file:\n";
    ss << "    a) First reply: LISTFILE:<ext>\n";
    ss << "    b) Then use EDITFILE / EDITEXCEL / EDITWORD with FULL PATH\n";
    ss << "26. When user says '打开' + existing file, use OPENFILE:<path>.\n";
    ss << "27. When user wants to read a file, use READFILE:<path>.\n";
    ss << "28. Use FULL PATH in EDIT / APPEND / OPENFILE / READFILE.\n";
    ss << "29. *** YOU ARE THE WRITER ***\n";
    ss << "    When the user gives a TOPIC (not the content), you MUST generate the actual text.\n";
    ss << "    - 'write 600-word essay on X'  ->  CREATEWORD:name;;;<ACTUAL 600-char essay in Chinese>\n";
    ss << "    - 'write a letter to Y'        ->  CREATEWORD:letter;;;<ACTUAL letter text>\n";
    ss << "    - 'create a Word file about Z' ->  CREATEWORD:z;;;<ACTUAL content about Z>\n";
    ss << "    NEVER copy the task description as content.\n";
    ss << "    NEVER write 'write 600-word essay on X' as the content.\n";
    ss << "    You are the author. Produce real Chinese text.\n";
    ss << "30. *** TOO LONG? MULTI-STEP ***\n";
    ss << "    If content > 1500 chars, split into 2-3 steps:\n";
    ss << "      Step 1: CREATEWORD:<name>;;;<part 1: opening>\n";
    ss << "      Step 2: APPENDWORD:<fullPath>;;;<part 2: body>\n";
    ss << "      Step 3: APPENDWORD:<fullPath>;;;<part 3: ending>\n";
    ss << "      Step 4: DONE\n";
    ss << "    Same for Excel (APPENDEXCEL) and txt (APPENDFILE).\n";
    ss << "31. NEVER include ';;;' inside content. It is the separator.\n";
    ss << "=== END HARD RULES ===\n\n";
    return ss.str();
}

// ========== Module 5: App Hints ==========
std::string ModuleAppHints(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== APP-SPECIFIC HINTS ===\n";

    ss << "[Browser]:\n";
    ss << "  - ALWAYS use Edge (OPEN:edge).\n";

    ss << "[Windows settings]:\n";
    ss << "  - OPEN:ms-settings:\n";

    ss << "[Open QQ]:\n";
    ss << "  - OPEN:qq\n";

    ss << "[Create file]:\n";
    ss << "  - Folder: CREATEFOLDER:<name>\n";
    ss << "  - txt/bat: CREATEFILE:<ext>;;;<name>;;;<content>\n";
    ss << "  - Excel: CREATEEXCEL:<name>;;;<row1>;;;<row2>...\n";
    ss << "  - Word: CREATEWORD:<name>;;;<p1>;;;<p2>...\n";

    ss << "[Modify / Append]:\n";
    ss << "  - Modify txt/bat: EDITFILE:<fullPath>;;;<newContent>\n";
    ss << "  - Append txt/bat: APPENDFILE:<fullPath>;;;<more>\n";
    ss << "  - Modify Excel:   EDITEXCEL:<fullPath>;;;<rows>\n";
    ss << "  - Append Excel:   APPENDEXCEL:<fullPath>;;;<newRow>\n";
    ss << "  - Modify Word:    EDITWORD:<fullPath>;;;<paragraphs>\n";
    ss << "  - Append Word:    APPENDWORD:<fullPath>;;;<newParagraph>\n";
    ss << "  - Open existing:  OPENFILE:<fullPath>\n";
    ss << "  - Read content:   READFILE:<fullPath>\n";
    ss << "  - List files:     LISTFILE:<ext>\n";

    ss << "[Writing Tasks]:\n";
    ss << "  - The user gives the TOPIC. You write the CONTENT.\n";
    ss << "  - 'write essay about X' -> you compose the essay in Chinese.\n";
    ss << "  - 'write letter to Y'   -> you compose the letter.\n";

    ss << "[BAT scripts]:\n";
    ss << "  - Content MUST be a REAL command.\n";
    ss << "  - Do NOT start with '@echo off'.\n";

    ss << "=== END APP HINTS ===\n\n";
    return ss.str();
}

// ========== Module 6: Reference ==========
std::string ModuleReferenceExamples(const PromptContext& ctx) {
    std::ostringstream ss;
    ss << "=== REFERENCE (do NOT copy) ===\n";
    ss << "  'open browser'       -> OPEN:edge -> DONE\n";
    ss << "  'open settings'      -> OPEN:ms-settings: -> DONE\n";
    ss << "  'make folder abc'    -> CREATEFOLDER:abc\n";
    ss << "  'make txt note'      -> CREATEFILE:txt;;;note;;;hello world\n";
    ss << "  'make excel report'  -> CREATEEXCEL:report;;;Name|Age;;;Alice|30\n";
    ss << "  'modify test.txt'    -> LISTFILE:txt -> EDITFILE:C:\\...\\test.txt;;;new content\n";
    ss << "\n";
    ss << "  Writing task example 1:\n";
    ss << "  User: '写一篇600字作文，主题成长'\n";
    ss << "  You should reply (about 200 chars per step, split into 3 steps):\n";
    ss << "    Step 1: CREATEWORD:growth_essay;;;成长，是一场漫长而美丽的旅程。每个人都在成长的道路上经历着属于自己的故事。有人在挫折中学会了坚强，有人在失去中懂得了珍惜，而我在失败与成功之间，渐渐找到了自己的方向。\n";
    ss << "    Step 2: APPENDWORD:C:\\Users\\Administrator\\Desktop\\growth_essay.docx;;;小时候的我，总是害怕犯错。一次考试失利就能让我哭上半天，一句批评就能让我怀疑自己。可是后来我明白了，失败并不是终点，而是通往成功的必经之路。每一次跌倒，都让我学会了如何站得更稳。\n";
    ss << "    Step 3: APPENDWORD:C:\\Users\\Administrator\\Desktop\\growth_essay.docx;;;如今的我不再畏惧挑战。我知道成长不仅仅是年龄的增长，更是心智的成熟。它教会我勇敢、坚韧与包容。成长没有终点，只有不断前行的旅程。\n";
    ss << "    Step 4: DONE\n";
    ss << "\n";
    ss << "  Writing task example 2:\n";
    ss << "  User: '写一封给老师的感谢信'\n";
    ss << "  You should reply:\n";
    ss << "    CREATEWORD:thank_you_letter;;;尊敬的李老师：;;;您好！感谢您这几年来的辛勤教导。您不仅教会了我知识，更教会了我如何做人。;;;每当我遇到困难时，您总是耐心地鼓励我；每当我取得进步时，您的笑容是我最大的动力。;;;如今我即将毕业，心中满怀不舍与感激。愿您身体健康，桃李满天下！;;;此致 敬礼;;;您的学生 张三\n";
    ss << "\n";
    ss << "  'what is python'     -> CHAT:Python is a programming language...\n";
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
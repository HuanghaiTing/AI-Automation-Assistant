#pragma once

// 启动日志 tailer 线程：监视 app.log，实时推给前端
void StartLogTailer();

// 停止线程（退出时调用）
void StopLogTailer();
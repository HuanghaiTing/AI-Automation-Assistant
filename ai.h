#pragma once
#include "common.h"

// 截图（base64）
std::string CaptureScreenBase64();

// 调 AI 视觉模型（图片 + 文字）
std::string CallAIVision(const std::string& prompt, const std::string& imageBase64);

// ★ 用户是否点了"停止"
bool IsStopRequested();

// ★ 立刻中断正在进行的 AI 请求
void AbortCurrentAIRequest();
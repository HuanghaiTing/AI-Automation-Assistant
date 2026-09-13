#pragma once
#include "common.h"

bool FindIconByAI(const std::string& label, int& outX, int& outY);
bool FindMenuTextByAI(const std::string& label, int& outX, int& outY);
bool FindInputByAI(const std::string& hint, int& outX, int& outY);
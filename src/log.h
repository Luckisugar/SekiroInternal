#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void LogBind(HMODULE module);
void Log(const char* line);
void Logf(const char* fmt, ...);
void LogFail(const char* line);
const wchar_t* LogDir();
int LogCopy(char* dest, int destBytes, int maxLines);

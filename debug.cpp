#include "debug.h"

#include <windows.h>
#include <stdio.h>

void debug_log(const char *format, ...) {
    va_list argptr;
    va_start(argptr, format);
    char str[1024];
    vsprintf_s(str, sizeof(str), format, argptr);
    va_end(argptr);
    OutputDebugString(str);
}

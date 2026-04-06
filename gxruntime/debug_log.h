#pragma once
#include <cstdio>

inline void dbg_log(const char* msg) {
    static FILE* logFile = nullptr;
    if (!logFile) {
        logFile = fopen("gxgraphics_debug.log", "w");
    }
    if (logFile) {
        fprintf(logFile, "[DBG] %s\n", msg);
        fflush(logFile);
    }
}
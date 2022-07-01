#include "Log.hpp"
#include "../includes.hpp"

#include <fstream>
#include <iostream>

void Debug::log(LogLevel level, const char* fmt, ...) {

    std::string levelstr = "";

    switch (level) {
        case LOG:
            levelstr = "[LOG] ";
            break;
        case WARN:
            levelstr = "[WARN] ";
            break;
        case ERR:
            levelstr = "[ERR] ";
            break;
        case CRIT:
            levelstr = "[CRITICAL] ";
            break;
        case INFO:
            levelstr = "[INFO] ";
            break;
        default:
            break;
    }

    char buf[LOGMESSAGESIZE] = "";
    char* outputStr;
    int logLen;

    va_list args;
    va_start(args, fmt);
    logLen = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    if ((long unsigned int)logLen < sizeof buf) {
        outputStr = strdup(buf);
    } else {
        outputStr = (char*)malloc(logLen + 1);

        if (!outputStr) {
            printf("CRITICAL: Cannot alloc size %d for log! (Out of memory?)", logLen + 1);
            return;
        }

        va_start(args, fmt);
        vsnprintf(outputStr, logLen + 1U, fmt, args);
        va_end(args);
    }

    // hyprpaper only logs to stdout
    std::cout << levelstr << outputStr << "\n";

    // free the log
    free(outputStr);
}

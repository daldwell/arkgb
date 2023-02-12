#include <stdlib.h>

#pragma once

enum Severity
{
    EMPTY = 0,
    INFO,
    ERROR,
    DEBUG
};

struct LogItem
{
    const char * msg;
    Severity severity;
};

void Log(const char * msg, Severity Severity);
int ReadLogs(LogItem * readLogBuffer, int count);
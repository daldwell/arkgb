#include <stdlib.h>
#include "Logger.h"

#define MAX_LOG 1024

LogItem logs[MAX_LOG];
int logTail;


void Log(const char * msg, Severity Severity)
{
    logTail = (logTail+1)&(MAX_LOG-1);
    logs[logTail].msg = msg;
    logs[logTail].severity = Severity;
}

int ReadLogs(LogItem * readLogBuffer, int count)
{
    int logReadCount = 0;

    int logSeek;
    for (count; count > 0; count--) {
        logSeek = (logTail-(count-1))&(MAX_LOG-1);

        if (logs[logSeek].severity == EMPTY)
            continue;

        readLogBuffer[logReadCount] = logs[logSeek];
        logReadCount++;
    }

    return logReadCount;
}
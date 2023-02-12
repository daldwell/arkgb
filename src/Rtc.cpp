#include "Typedefs.h"
#include "Rtc.h"
#include "Logger.h"

void RtcRegister::LatchClock()
{
    time_t now = time(0);
    tm  *localTime = localtime(&now);
    sec = localTime->tm_sec;
    min = localTime->tm_min;
    hour = localTime->tm_hour;
    dl = 0; // TODO
}

void RtcRegister::SetMap(word addr)
{
    switch (addr)
    {
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
            map = addr;
            break;
        default:
            Log("Unknown RTC address", ERROR);
            exit(1);
    }
}

byte * RtcRegister::GetMap()
{
    switch (map)
    {
        case 0x08:
            return &sec;
        case 0x09:
            return &min;
        case 0x0A:
            return &hour;
        case 0x0B:
            return &dl;
        case 0x0C:
            return &dh;
    }

    Log("Unknown RTC address", ERROR);
    exit(1);
}
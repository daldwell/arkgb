#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctime>
#include "Typedefs.h"

#pragma once

class ClockTimer {
    public:
        ClockTimer();
        long LatchClock();
        int GetSeconds();
        int GetMinute();
        int GetHour();
    private:
        tm * currCalTime;
};

extern ClockTimer clockTimer;

FILE * getFileHandle(const char * name, const char * mode, bool mustExist);
void closeFileHandle( FILE * handle );
void fileRead( void * buffer, long unsigned size, long unsigned count, FILE * handle );
void fileWrite( void * buffer, long unsigned size, long unsigned count, FILE * handle );
void changeExtension( char * name, const char * newext);

inline int IncRotate(int a, int b)
{
    if (++a == b) a = 0; 
    return a;
}  

inline int DecRotate(int a, int b)
{
    if (--a < 0) a = b; 
    return a;
}  
#include "Typedefs.h"

#pragma once

struct Registers
{
    union 
    {
        word AF;
        struct
        {
            byte F; 
            byte A;
        };
    };

    union 
    {
        word BC;
        struct
        {
            byte C; 
            byte B;
        };
    };  

    union 
    {
        word DE;
        struct
        {
            byte E; 
            byte D;
        };
    };    

    union 
    {
        word HL;
        struct
        {
            byte L; 
            byte H;
        };
    };       

    word SP;
    word PC;
    // Not a part of the real CPU set, helper to determine if a jump has been executed
    bool jmp;
};

enum Flags
{
    ZF = 0,
    NF,
    HF,
    CF
};

extern struct Registers regs;
//extern struct Flags flags;
extern int cpuCycles;
extern bool cpuRunning;
extern bool halt;

void cpuStart();
void cpuStop();
void cpuCycle();
void cpuReset();
void setFlag(Flags, bool);
bool getFlag(Flags);


#include "Typedefs.h"

#pragma once

#define MAX_TRACE_LINES 0xDfff
#define MAX_DEBUG_LINES 0xefff
#define MAX_ROM_PC 0xDfff

struct DebugItem
{
    const char * code;
    int cycles;
    int pc;
    int sp;
    word spValue;
    int length;
    byte opcode;
    byte operands[2];
    byte romBank;
    word AF;
    word BC;
    word DE;
    word HL;
    byte IFRegister;
    byte TIMA;
};

extern DebugItem debugLines[MAX_DEBUG_LINES];

void addTraceLine(const char * code, int cycles, int pc, int sp, word spValue, int length, byte romBank);
void dumpTraceLines();
void refreshDebugLines(word pc, word end);
void initDebugger();
int getDebugLine();

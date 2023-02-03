#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Debugger.h"
#include "Mmu.h"
#include "Cpu.h"
#include "Rom.h"
#include "Opc.h"
#include "Misc.h"
#include "Timer.h"
#include "GUnit.h"

#define HEADER_BEGIN 0x104
#define HEADER_END 0x14f

DebugItem traceLines[MAX_DEBUG_LINES];
int traceTail = 0;
DebugItem debugLines[MAX_DEBUG_LINES];
int debugIndex[MAX_DEBUG_LINES];

void addTraceLine(const char * code, int cycles, int pc, int sp, word spValue, int length, byte romBank) {
    traceLines[traceTail].romBank = romBank;
    traceLines[traceTail].cycles = cycles;
    traceLines[traceTail].code = code;
    traceLines[traceTail].pc = pc;
    traceLines[traceTail].sp = sp;
    traceLines[traceTail].spValue = spValue;
    traceLines[traceTail].IFRegister = IFRegister;
    traceLines[traceTail].TIMA = timeRegs.TIMA;

    for (int i = 1; i < length; i++) {
        traceLines[traceTail].operands[i-1] = mmu.PeekByte(pc+i);
    }
    traceTail = IncRotate(traceTail, MAX_TRACE_LINES);
}

void dumpTraceLines() {

    for (int i = 0; i < MAX_TRACE_LINES; i++) {
        traceTail = DecRotate(traceTail, MAX_TRACE_LINES);
        printf("Rom Bank: %x, cycles %x, PC: %x, SP: %x, SP value %x, Code: %s, op1 %x, op2 %x, IF %x, TIMA %x\n", traceLines[traceTail].romBank, traceLines[traceTail].cycles, traceLines[traceTail].pc, traceLines[traceTail].sp, traceLines[traceTail].spValue, traceLines[traceTail].code, traceLines[traceTail].operands[0], traceLines[traceTail].operands[1], traceLines[traceTail].IFRegister, traceLines[traceTail].TIMA);
    }
}

void refreshDebugLines(word pc, word end)
{
    int i = 0;
    byte opc;
    Instruction instr;
    DebugItem * dbgl;

    //assert (rom is loaded)

    while (pc <= end) {
        opc = mmu.PeekByte(pc);
        debugIndex[pc] = i;
        dbgl = &debugLines[i++];
        dbgl->opcode = opc;
        dbgl->pc = pc;

        // Special case for handling gb header data
        if (pc >= HEADER_BEGIN && pc <= HEADER_END) {
            dbgl->code = "db";
            dbgl->operands[0] = opc;
            pc++;
            continue;
        }

        // Populate debug line
        instr = instTbl[opc];

        // Handle undefined opcode
        if (instr.code == NULL) {
            dbgl->code = "undefined instruction";
            pc++;
            continue;
        }

        dbgl->code = instr.code;
        dbgl->length = instr.length;

        for (int i = 1; i < instr.length; i++) {
            dbgl->operands[i-1] = mmu.PeekByte(pc+i);
        }

        pc += instr.length;
    }
}

void initDebugger()
{
    refreshDebugLines(0, MAX_ROM_PC);
}

int getDebugLine()
{
    return debugIndex[regs.PC];
}
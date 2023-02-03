#include "display.h"
#include "cpu.h"
#include "mmu.h"
#include "opc.h"
#include "interrupt.h"
#include "timer.h"
#include "audio.h"
#include "GUnit.h"


int cpuCycles;
bool cpuRunning;
bool halt;

// TODO: move to the GComponent class pattern
void cpuCycle()
{
    cpuCycles = 0;
    interrupt.Cycle();
    instTbl[mmu.PeekByte(regs.PC)].step();
    timer.Cycle();
    display.Cycle();
    audio.Cycle();
}

void cpuReset()
{
    cpuCycles = 0;
    displayCycles = 200;

    // DMG startup values
    // TODO: profiles for other GB types

    // Registers
    regs.AF = 0x0100;
    regs.BC = 0x0013;
    regs.DE = 0x00D8;
    regs.HL = 0x014D;
    regs.SP = 0xFFFE;
    regs.PC = 0x100;

    setFlag(CF, 0);
    setFlag(HF, 0);
    setFlag(NF, 0);
    setFlag(ZF, 1);

    // Timer
    timeRegs.DIV = 0xAB00;
    timeRegs.TIMA = 0x00;
    timeRegs.TMA = 0x00;
    timeRegs.TAC = 0xF8;

    // Interrupt
    IFRegister = 0xE1;
    IERegister = 0x0;

    // Display
    lcdRegs.LCDC = 0x91; 
    lcdRegs.STAT = 0x85;
    lcdRegs.SCY = 0x0;
    lcdRegs.SCX = 0x0;
    lcdRegs.LY = 0x91;
    lcdRegs.LYC = 0x0;
    lcdRegs.DMA = 0xFF;
    lcdRegs.BGP = 0xFC;
    lcdRegs.OBP0 = 0x0;
    lcdRegs.OBP1 = 0x0;
    lcdRegs.WX = 0x0;
    lcdRegs.WY = 0x0;

}
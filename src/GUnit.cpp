#include "GUnit.h"
#include "Mmu.h"
#include "Audio.h"
#include "Timer.h"
#include "Interrupt.h"
#include "Cpu.h"
#include "Control.h"
#include "Rom.h"
#include "Opc.h"

MmuComponent mmu;
CpuComponent cpu;
AudioComponent audio;
DisplayComponent display;
TimerComponent timer;
InterruptComponent interrupt;
ControlComponent control;

void GUInit(char * rom)
{
    initOpc();
    //executeTests();

    loadRom(rom);
}

void GUCycle()
{
    cpuCycles = 0;
    interrupt.Cycle();
    cpu.Cycle();
    timer.Cycle();
    display.Cycle();
    audio.Cycle();
}

void GUReset()
{
    cpuCycles = 0;
    interrupt.Reset();
    cpu.Reset();
    timer.Reset();
    display.Reset();
    audio.Reset();
}

void GUShutdown()
{
    closeRom();
}

void GUSetProfile(Profile profile)
{
    cpu.profile = profile;
    display.profile = profile;
}

void GUDispatchEvent(SDL_Event * e)
{
    audio.EventHandler(e);
    control.EventHandler(e);
}
#include "GUnit.h"
#include "Mmu.h"
#include "Audio.h"
#include "Timer.h"
#include "Interrupt.h"
#include "Cpu.h"

MmuComponent mmu;
CpuComponent cpu;
AudioComponent audio;
DisplayComponent display;
TimerComponent timer;
InterruptComponent interrupt;

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

void GUProfile(Profile profile)
{
    cpu.profile = profile;
    display.profile = profile;
}
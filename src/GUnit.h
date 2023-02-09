#include "Mmu.h"
#include "Display.h"
#include "Audio.h"
#include "Timer.h"
#include "Interrupt.h"
#include "Cpu.h"

#pragma once

extern MmuComponent mmu;
extern CpuComponent cpu;
extern AudioComponent audio;
extern DisplayComponent display;
extern TimerComponent timer;
extern InterruptComponent interrupt;

void GUCycle();
void GUReset();
void GUProfile(Profile profile);
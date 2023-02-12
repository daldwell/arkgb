#include "Mmu.h"
#include "Display.h"
#include "Audio.h"
#include "Timer.h"
#include "Interrupt.h"
#include "Cpu.h"
#include "Control.h"
#include "Rom.h"

#pragma once

extern MmuComponent mmu;
extern CpuComponent cpu;
extern AudioComponent audio;
extern DisplayComponent display;
extern TimerComponent timer;
extern InterruptComponent interrupt;
extern ControlComponent control;
extern RomComponent romComponent;

void GUInit(char *);
void GUCycle();
void GUReset();
void GUShutdown();
void GUSetProfile(Profile);
void GUDispatchEvent(SDL_Event *);
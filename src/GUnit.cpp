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
RomComponent romComponent;

void GUInit(char * romName)
{
    romComponent.Close();
    romComponent.Load(romName);
    GUReset();
    cpuRunning = true;
}

void GUCycle()
{
    if (cpuRunning) {
        cpuCycles = 0;
        interrupt.Cycle();
        cpu.Cycle();
        timer.Cycle();
        audio.Cycle();
        display.Cycle();

        if (display.frameReady >= 1 && display.frameReady > speedFactor) {
            // If speed is negative we duplicate the frame to slow things down
            int c = (speedFactor < 0) ? (speedFactor*-1)+1 : 1;
            for (int i = 0; i < c; i++) {
                gwindow.RefreshWindow();
            }
            display.frameReady = 0;
        }  
    } else {
        // Just render the GUI
        gwindow.RefreshWindow();
    }
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
    romComponent.Close();
}

void GUSetProfile(Profile profile)
{
    cpu.profile = profile;
    display.profile = profile;
}

void GUDispatchEvent(SDL_Event * e)
{
    display.EventHandler(e);
    audio.EventHandler(e);
    control.EventHandler(e);
}
#include "Window.h"
#include "Rom.h"
#include "Logger.h"
#include "Debugger.h"
#include "Tester.h"
#include "Opc.h"
#include "Cpu.h"
#include "Audio.h"

int main(int argv, char** args)
{
    if (argv != 2) {
        Log("Usage: ArkGB.exe rom.gb(c) ", INFO);
        return 0;
    }

    Log("Welcome to ArkGB!", INFO);

    GUInit(args[1]);
    GUReset();
    initDebugger();
    cpuRunning = true;
    while (gwindow.running) {
        if (cpuRunning) {
            GUCycle();
        } 
    }
    
    return 0;
}
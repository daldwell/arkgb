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
        printf("Usage: ArkGB.exe rom.gb(c) ");
        return 0;
    }

    Log("Welcome to ArkGB!", INFO);

    initOpc();
    //executeTests();

    loadRom(args[1]);
    cpuReset();
    initDebugger();
    cpuRunning = true;
    while (gwindow.running) {
        if (cpuRunning) {
            cpuCycle();
        } 
    }
    
    return 0;
}
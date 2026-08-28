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
    
    Log("Welcome to ArkGB!", INFO);
    Log("Usage: ArkGB.exe rom.gb(c) ", INFO);

    initOpc();
    Log("Opcodes initialised", INFO);
    if (argv > 1) { GUInit(args[1]); } // If a ROM was supplied as argument
    Log("Supplied rom loaded", INFO);
    Log("Beginning main loop", INFO);
    while (gwindow.running) {
        GUCycle();
    }
    Log("Thank you and goodnight!", INFO);
    return 0;
}
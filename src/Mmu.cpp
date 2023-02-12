#include <algorithm>
#include <stdio.h>
#include "Mmu.h"
#include "Typedefs.h"
#include "Display.h"
#include "Interrupt.h"
#include "Serial.h"
#include "Timer.h"
#include "Control.h"
#include "Cpu.h"
#include "Audio.h"
#include "Rom.h"
#include "Debugger.h"
#include "GUnit.h"
#include "Logger.h"

byte vram[1][0x2000]; 
byte vram_bnk_no;
byte wram[0x7][0x1000]; 
byte wram_bnk_no = 1;
byte hram[0x100]; 
byte cram[0x100]; 
byte unmapped = 0xFF;

void MmuComponent::EventHandler(SDL_Event * e)
{
    // Not implemented
}

byte MmuComponent::PeekByte(word addr)
{
    // ROM
    if (romComponent.MemoryMapped(addr)) {
        //printf("PEEK ROM %x\n", addr);
        return romComponent.PeekByte(addr);
    }

    // Joypad registers
    if (control.MemoryMapped(addr)) {
        return control.PeekByte(addr);
    }

    // Cpu registers
    if (cpu.MemoryMapped(addr)) {
        return cpu.PeekByte(addr);
    }

    // Audio registers
    if (audio.MemoryMapped(addr)) {
        return audio.PeekByte(addr);
    }

    //LCD registers
    if (display.MemoryMapped(addr)) {
        return display.PeekByte(addr);
    }

    //Interrupt register
    if (interrupt.MemoryMapped(addr)) {
        return interrupt.PeekByte(addr);
    }

    return *memoryMap(addr);
}

void MmuComponent::PokeByte(word addr, byte value)
{ 

    // ROM
    if (romComponent.MemoryMapped(addr)) {
        romComponent.PokeByte(addr, value);
        return;
    }

    if (addr == 0xFF70) {
        wram_bnk_no = std::max(1, (value&0x7));
    }

    // Joypad registers
    if (control.MemoryMapped(addr)) {
        control.PokeByte(addr, value);
        return;
    }

    // Cpu registers
    if (cpu.MemoryMapped(addr)) {
        cpu.PokeByte(addr, value);
        return;
    }

    if (audio.MemoryMapped(addr)) {
        audio.PokeByte(addr, value);
        return;
    }

    if (display.MemoryMapped(addr)) {
        display.PokeByte(addr, value);
        return;
    }

    //Interrupt register
    if (interrupt.MemoryMapped(addr)) {
        interrupt.PokeByte(addr, value);
        return;
    }

    *memoryMap(addr) = value;
}

word MmuComponent::PeekWord(word addr)
{
    return ((word)PeekByte(addr+1) << 8) + PeekByte(addr);
}

void MmuComponent::PokeWord(word addr, word value)
{
    PokeByte(addr, value);
    PokeByte(addr+1, value>>8);
}

void MmuComponent::Cycle()
{
    // Not implemented
}

void MmuComponent::Reset()
{
    // Not implemented
}

bool MmuComponent::MemoryMapped(word addr)
{
    if (addr <= 0xFFFF )
        return true;

    return false;
}

/*
This method is deprecated and will be phased out in favour of GComponent Peek/Poke byte
*/
byte * memoryMap(word addr)
{
    switch (addr&0xF000)
    {
        // Working ram
        case 0xC000:
            return &wram[0][addr&0x1FFF];
        case 0xD000:
            return &wram[wram_bnk_no][addr&0x1FFF];
        // Shadow working ram
        case 0xE000:
            return &wram[0][addr&0xFFF];
        case 0xF000:
     
            //TODO: sound, IO registers
            if (addr >= 0xFEA0 && addr <= 0xFF3F) {

                if (addr <= 0xFEFF)
                    return &cram[addr&0xFF];

                // Serial data
                if (addr == 0xFF01)
                    return &serialData; 

                if (addr == 0xFF01)
                    return &serialData; 

                if (addr == 0xFF02)
                    return &serialTransfer; 

                // Timer registers
                if (addr >= 0xFF04 && addr <= 0xFF07) {
                    switch (addr) {
                        case 0xFF04:
                            return ((byte*)&timeRegs.DIV)+1;
                        case 0xFF05:
                            return &timeRegs.TIMA;
                        case 0xFF06:
                            return &timeRegs.TMA;
                        case 0xFF07:
                            return &timeRegs.TAC;
                    }
                }

                return &unmapped;
            }

            //Unused registers (TODO: GBC)
            if (addr >= 0xFF4C && addr <= 0xFF7F) {
                //return &cram[addr&0x7F];
                return &unmapped;
            }

            // High Ram
            if (addr >= 0xFF80 && addr < 0xFFFF) {
                return &hram[addr&0x7F];
            }
            
            return &unmapped;
            // TODO: move to error handler
            Log("Error: memory could not be mapped", ERROR);
            printf("Memory %x", addr);

            exit(1);
        default:
            Log("Error: memory could not be mapped", ERROR);
            printf("Memory %x", addr);

            exit(1);
    }
}
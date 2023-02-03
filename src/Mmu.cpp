#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
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

byte rom[0x4000];
byte rom_bnk[0xFF][0x4000];
byte rom_bnk_no;
byte vram[1][0x2000]; 
byte ram[0x4][0x2000]; 
byte ram_bnk_no;
byte wram[0x2000]; 
byte hram[0x100]; 
byte cram[0x100]; 
byte unmapped = 0xFF;
RamMode ramMode;

byte MmuComponent::PeekByte(word addr)
{
    // TODO: workaround for JOYP register - select either the action/direction bit plane depending on register value
    if (addr == 0xFF00) {
        byte * joy = memoryMap(addr);

        if (! (*joy & (1<<4))) {
            return (*joy&0xF0) + Direction;
        } else if (! (*joy & (1<<5))) {
            return (*joy&0xF0) + Action;
        } else {
            return 0xFF;
        }
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
    // TODO: move this logic into the ROM module
    // MBC1
    if (romHeader->cartType >= 0x1 && romHeader->cartType <= 0x3) {
        // Byte written to ROM space - this updates ROM banking register
        if (addr < 0x1FFF) {
            // Toggle cart RAM register
            ramMode = (value == 0xA ? RamEnabled : RamDisabled);
            return;
        } else if (addr < 0x3FFF) {
            // Set ROM bank
            rom_bnk_no = (value&0x1F) > 0 ? (value&0x1F)-1 : 0;
            return;
        } else if (addr < 0x5FFF) {
            // Set RAM bank
            // TODO: handle Banking select mode
            ram_bnk_no = (value&0x3) > 0 ? (value&0x3)-1 : 0;
            return;
        } else if (addr < 0x7FFF) {
            // TODO: handle Banking select mode
            // assert(0==1);
            return;
        }
    } 
    // MBC 3
    else if (romHeader->cartType >= 0xF && romHeader->cartType <= 0x13) {
        // Byte written to ROM space - this updates ROM banking register
        if (addr < 0x1FFF) {
            // Toggle cart RAM register
            ramMode = (value == 0xA ? RamEnabled : RamDisabled);
            return;
        } else if (addr < 0x3FFF) {
            // Set ROM bank
            rom_bnk_no = (value&0x7F) > 0 ? (value&0x7F)-1 : 0;
            return;
        } else if (addr < 0x5FFF) {
            // Set RAM bank
            // TODO: handle real time clock
            if ((value) >= 0x8 && (value) <= 0xC ) {
                ramMode = RamRtc;
                return;
            } 

            ram_bnk_no = (value&0x3) > 0 ? (value&0x3)-1 : 0;
            return;
        } else if (addr < 0x7FFF) {
            // TODO: handle real time clock
            // printf("address: %x value: %x PC %x rombank %x rambank %x\n", addr, value, regs.PC, rom_bnk_no, ram_bnk_no);
            // assert(0==1);
            return;
        }
    }
    // Default (should be MBC 0) 
    else {
        if (addr < 0x7FFF) {
            // Block ROM writes
            return;
        }
    }

    // TODO: workaround for JOYP register as this contains read-only bits that should not be overwritten
    if (addr == 0xFF00) {
        byte * joy = memoryMap(addr);
        *joy = (value&0xF0) + (*joy&0xF);
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

byte * memoryMap(word addr)
{
    switch (addr&0xF000)
    {

        // Rom bank 0
        // TODO: add startup rom
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
            return &rom[addr];
        // Rom bank 1..n
        // TODO: more than bank 1
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:        
            return &rom_bnk[rom_bnk_no][addr&0x3FFF];
        // Vram
        // TODO: vram banks
        case 0x8000:
        case 0x9000:
            return &vram[0][addr&0x1FFF];
        // Cart ram
        case 0xA000:
        case 0xB000:
            if (ramMode == RamEnabled) {
                return &ram[ram_bnk_no][addr&0x1FFF];
            } else {
                return &unmapped;
            }
        // Working ram
        case 0xC000:
        case 0xD000:
            return &wram[addr&0x1FFF];
        // Shadow working ram
        case 0xE000:
            return &wram[addr&0xFFF];
        // Upper bound of Shadow Working ram
        case 0xF000:
     
            //TODO: sound, IO registers
            if (addr >= 0xFEA0 && addr <= 0xFF3F) {

                if (addr <= 0xFEFF)
                    return &cram[addr&0xFF];

                // Joypad
                if (addr == 0xFF00) {
                    return &JOYP; 
                }

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

                //assert(2==0);
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
            printf("Error: memory could not be mapped: %x", addr);
            exit(1);
        default:
            printf("Error: memory could not be mapped: %x", addr);
            exit(1);
    }
}
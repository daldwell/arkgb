#include "GComponent.h"
#include "Typedefs.h"

#pragma once

class MmuComponent : GComponent
{
    public:
        MmuComponent() {};
        ~MmuComponent() {};
        void PokeByte(word, byte);
        byte PeekByte(word);
        void PokeWord(word, word);
        word PeekWord(word);
        void Cycle();
        void Reset();
    protected:
        bool MemoryMapped(word);
};

byte * memoryMap(word addr);
extern byte unmapped;

extern byte rom[0x4000];
extern byte rom_bnk[0xFF][0x4000];
extern byte rom_bnk_no;
extern byte vram[1][0x2000]; 
extern byte ram[0x4][0x2000]; 
extern byte wram[0x2000]; 
extern byte hram[0x100]; 

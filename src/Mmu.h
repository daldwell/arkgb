#include "GComponent.h"
#include "Typedefs.h"

#pragma once

class MmuComponent : public GComponent
{
    public:
        MmuComponent() {};
        ~MmuComponent() {};
        void EventHandler(SDL_Event *) override;
        void PokeByte(word, byte) override;
        byte PeekByte(word) override;
        void PokeWord(word, word);
        word PeekWord(word);
        void Cycle() override;
        void Reset() override;
    protected:
        bool MemoryMapped(word);
};

class RtcClock
{
    public:
        void LatchClock();
        void PeekByte();
        void PokeByte();
    private:
        void GetSeconds();
        void GetMinutes();
        void GetHours();
};

byte * memoryMap(word addr);
extern byte unmapped;

extern byte vram[2][0x2000]; 
extern byte vram_bnk_no;
extern byte wram[8][0x1000]; 
extern byte wram_bnk_no;
extern byte hram[0x100]; 

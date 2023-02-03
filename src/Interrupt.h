#include <stdlib.h>
#include "Typedefs.h"
#include "GComponent.h"

#pragma once

#define vblank_flag 0x1
#define lcd_flag 0x1<<1
#define timer_flag 0x1<<2
#define serial_flag 0x1<<3
#define joypad_flag 0x1<<4

class InterruptComponent : GComponent
{
    public:
        InterruptComponent() {};
        ~InterruptComponent() {};
        void PokeByte(word, byte);
        byte PeekByte(word);
        void Cycle();
        void Reset();
    protected:
        bool MemoryMapped(word);
    friend class MmuComponent;
};


extern byte IERegister;
extern byte IFRegister;

extern int IMECounter;
extern bool IMERegister;


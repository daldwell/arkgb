#include "Typedefs.h"
#include "GComponent.h"

#pragma once

class TimerComponent : public GComponent
{
    public:
        TimerComponent() {};
        ~TimerComponent() {};
        void PokeByte(word, byte);
        byte PeekByte(word);
        void Cycle();
        void Reset();
    protected:
        bool MemoryMapped(word);
    friend class MmuComponent;
};

struct TimerRegister
{
    word DIV;
    byte TIMA;
    byte TMA;
    byte TAC;
};

struct TAC
{
    word mask;
    bool edge;
};

extern struct TimerRegister timeRegs;
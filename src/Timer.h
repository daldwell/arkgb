#include "Typedefs.h"
#include "GComponent.h"

#pragma once

class TimerComponent : public GComponent
{
    public:
        TimerComponent() {};
        ~TimerComponent() {};
        void EventHandler(SDL_Event *) override;
        void PokeByte(word, byte) override;
        byte PeekByte(word) override;
        void Cycle() override;
        void Reset() override;
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
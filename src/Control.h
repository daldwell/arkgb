#include "Typedefs.h"
#include "GComponent.h"

#pragma once

class ControlComponent : public GComponent
{
    public:
        ControlComponent() {};
        ~ControlComponent() {};
        void EventHandler(SDL_Event *) override;
        void PokeByte(word, byte) override;
        byte PeekByte(word) override;
        void Cycle() override;
        void Reset() override;
    protected:
        bool MemoryMapped(word);
        friend class MmuComponent;
};


enum JoyPadKey
{
    Up = 0,
    Down,
    Left,
    Right,
    Start,
    Select,
    A,
    B,
    None
};


extern byte JOYP;
extern byte Direction;
extern byte Action;

void ProcessKey(JoyPadKey, bool);
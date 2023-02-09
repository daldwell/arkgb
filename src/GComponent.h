#include "Typedefs.h"

#pragma once

enum Profile
{
    DMG = 0,
    CGB
};

class GComponent
{
    public:
        GComponent() {};
        ~GComponent() {};
        virtual void PokeByte(word, byte) = 0;
        virtual byte PeekByte(word) = 0;
        virtual void Cycle() = 0;
        virtual void Reset() = 0;
        Profile profile;
    protected:
        virtual bool MemoryMapped(word) = 0;
};
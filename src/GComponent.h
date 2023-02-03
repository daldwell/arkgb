#include "Typedefs.h"

#pragma once

class GComponent
{
    public:
        GComponent() {};
        ~GComponent() {};
        virtual void PokeByte(word, byte) = 0;
        virtual byte PeekByte(word) = 0;
        virtual void Cycle() = 0;
        virtual void Reset() = 0;
    protected:
        virtual bool MemoryMapped(word) = 0;
};
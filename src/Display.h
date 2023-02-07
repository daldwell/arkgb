#include <stdlib.h>
#include "GComponent.h"
#include "Typedefs.h"

#pragma once

struct LcdRegister
{
    byte LCDC;
    byte STAT;
    byte SCY;
    byte SCX;
    byte LY;
    byte LYC;
    byte DMA;
    byte BGP;
    byte OBP0;
    byte OBP1;
    byte WY;
    byte WX;
};

struct Sprite
{
    byte yPos;
    byte xPos;
    byte tIdx;
    byte attr;
};

struct Pixel
{
    byte r;
    byte g;
    byte b;
};

class DisplayComponent : GComponent
{
    public:
        DisplayComponent() {};
        ~DisplayComponent() {};
        void PokeByte(word, byte);
        byte PeekByte(word);
        void PokeWord(word, word);
        word PeekWord(word);
        void Cycle();
        void Reset();
    protected:
        bool MemoryMapped(word);
        int GetColorFromPalette(Pixel);
        void DrawTileRow(byte, int, int, byte, byte, word, byte);
        void DrawSpritesRow(byte);
        void DrawBackgroundRow(byte);
        void DrawWindowRow(byte);
        friend class MmuComponent;
};

extern int displayCycles;

extern struct LcdRegister lcdRegs;
extern struct Sprite oamTable[40];
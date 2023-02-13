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

struct HdmaRegister
{
    word src;
    word dst;
    byte status;
};

class DisplayComponent : public GComponent
{
    public:
        DisplayComponent() {};
        ~DisplayComponent() {};
        void EventHandler(SDL_Event *) override;
        void PokeByte(word, byte) override;
        byte PeekByte(word) override;
        void Cycle() override;
        void Reset() override;
    protected:
        bool MemoryMapped(word);
        int GetColorFromPalette(Pixel);
        void DrawTileRow(byte, int, int, byte, byte, word, byte);
        void DrawSpritesRow(byte);
        void DrawBackgroundRow(byte);
        void DrawWindowRow(byte);
        LcdRegister lcdRegs;
        HdmaRegister hdmaRegs;
        Sprite oamTable[40];
        int displayCycles;
        friend class MmuComponent;
};

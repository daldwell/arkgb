#include "GComponent.h"
#include "Typedefs.h"
#include "Mbc.h"

#pragma once

class RomComponent : public GComponent
{
    public:
        RomComponent() {};
        ~RomComponent() {};
        void EventHandler(SDL_Event *) override;
        void PokeByte(word, byte) override;
        byte PeekByte(word) override;
        void Cycle() override;
        void Reset() override;
        void Load(const char *);
        void Close();
    protected:
        byte GetRamBanks();
        bool MemoryMapped(word);
        byte rom[0x1FF][0x4000];
        byte ram[0x10][0x2000]; 
        MbcBase * mbc;
        friend class MmuComponent;
};

struct RomHeader
{
    byte logo[0x30];
    char title[0x10];
    // byte manufactureCode[4]; This is not used and the bytes can be allocated to the title 
    word newLicensee;
    byte SGBflag;
    byte cartType;
    byte romSize;
    byte ramSize;
    byte destinationCode;
    byte oldLicensee;
    byte maskROM;
    byte headerChecksum;
    word globalChecksum;
};

extern struct RomHeader * romHeader;

#include "Typedefs.h"

#pragma once

struct RomHeader
{
    byte logo[0x30];
    char title[0x10];
    // byte manufactureCode[4]; This is not used and the bytes can be allocated to the title 
    // byte CGBflag; TODO: handle this when ArkGB supports CGB 
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

struct RtcRegister
{
    byte S;
    byte M;
    byte H;
    byte DL;
    byte DH;
};

enum RamMode {
    RamDisabled = 0,
    RamEnabled,
    RamRtc
};

extern struct RomHeader * romHeader;
extern byte rom_bnk_no;

void pokeRom(word addr, byte value);
byte * romMemoryMap(word addr);
byte * rtcMap(byte rtcRegNum);
void loadRom(const char *);
void closeRom();

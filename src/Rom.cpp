#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctime>
#include "Rom.h"
#include "Mmu.h"
#include "Typedefs.h"
#include "Logger.h"
#include "Misc.h"
#include "Window.h"
#include "GUnit.h"

char romName[100];
char ramName[100];
char buf[100];
struct RomHeader * romHeader;
RtcRegister rtc;

void RtcRegister::LatchClock()
{
    time_t now = time(0);
    tm  *localTime = localtime(&now);
    sec = localTime->tm_sec;
    min = localTime->tm_min;
    hour = localTime->tm_hour;
    dl = 0; // TODO
}

void RtcRegister::SetMap(word addr)
{
    switch (addr)
    {
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
        case 0x0C:
            map = addr;
            break;
        default:
            Log("Unknown RTC address", ERROR);
            exit(1);
    }
}

byte * RtcRegister::GetMap()
{
    switch (map)
    {
        case 0x08:
            return &sec;
        case 0x09:
            return &min;
        case 0x0A:
            return &hour;
        case 0x0B:
            return &dl;
        case 0x0C:
            return &dh;
    }

    Log("Unknown RTC address", ERROR);
    exit(1);
}

// byte RtcRegister::PeekByte(word addr)
// {
//     switch (addr)
//     {
//         case 0x08:
//             return sec;
//         case 0x09:
//             return min;
//         case 0x0A:
//             return hour;
//         case 0x0B:
//             return dl;
//         case 0x0C:
//             return dh;
//     }

//     Log("Unknown RTC address", ERROR);
//     exit(1);
// }

// void RtcRegister::PokeByte(word addr, byte value)
// {
//     // TODO: this is not accurate, writing should update the real "behind the scenes" clock, not just the latched registers
//     switch (addr)
//     {
//         case 0x08:
//             sec = value;
//             break;
//         case 0x09:
//             min = value;
//             break;
//         case 0x0A:
//             hour = value;
//             break;
//         case 0x0B:
//             dl = value;
//             break;
//         case 0x0C:
//             dh = value;
//             break;
//         default:
//             Log("Unknown RTC address", ERROR);
//             exit(1);
//     }

// }

static byte getRamBanks()
{
    byte banks;
    switch (romHeader->ramSize)
    {
        case 2:
            banks = 1;
            break;
        case 3:
            banks = 4;
            break;
        case 4:
            banks = 16;
            break;
        case 5:
            banks = 8;
            break;
        default:
            sprintf(buf, "Unknown RAM size code %x", romHeader->ramSize);
            Log(buf, ERROR);
            exit(1);
    }
            
    return banks;   
}

void loadRom(const char * rmt)
{
    FILE *romFile;
    FILE *ramFile;
    byte banks;
    
    strcpy(romName, rmt);
    romFile = getFileHandle(romName,"rb", true);

    sprintf(buf, "Rom %s loaded", romName);
    Log(buf, INFO);

    // Read bank 0 and 1 - all carts have these banks
    fileRead(&rom[0], sizeof(byte), 0x4000, romFile);
    fileRead(&rom_bnk[0][0], sizeof(byte), 0x4000, romFile);

    // Set up rom header
    romHeader = (RomHeader*)(&rom[0]+0x104);

    // Load rom
    switch (romHeader->cartType) {
        case 0:
            break;
        case 0x01:     // Handle MBC 1
        case 0x03:
        case 0x10:     // Handle MBC 3
        case 0x13:
        case 0x1b:     // Handle MBC 5

            // Read all banks
            for (int i = 1; i < (0x2 << romHeader->romSize)-1; i++) {
                fileRead(&rom_bnk[i][0], sizeof(byte), 0x4000, romFile);
            }
            break;
        default:
            sprintf(buf, "Unsupported MBC code %x", romHeader->cartType);
            Log(buf, ERROR);
            exit(1);
    }
 
    // Load ram
    switch (romHeader->cartType) {
        case 0:
            break;
        case 0x03:     // Handle MBC 1
        case 0x10:     // Handle MBC 3
        case 0x13:
        case 0x1b:     // Handle MBC 5
            // Read battery backed RAM
            strcpy(ramName, romName);
            changeExtension(ramName, "sav");
            ramFile = getFileHandle(ramName, "rb", false);

            if (ramFile != NULL) {
                banks = getRamBanks();
                for (int i = 0; i < banks; i++) {
                    fileRead(&ram[i][0], sizeof(byte), 0x2000, ramFile);
                }

                closeFileHandle(ramFile);
            }
            break;
        default:
            Log("Battery backed RAM not supported for this cart", INFO);
    }

    closeFileHandle(romFile);

    // Set window title to ROM title
    sprintf(buf, "ArkGB - %s", romHeader->title);
    gwindow.SetTitle(buf);

    // Set GBC profile if cart is compatible
    byte gbcFlag = romHeader->title[0xF];
    if (gbcFlag == 0x80 || gbcFlag == 0xC0) {
        GUSetProfile(CGB);
    }
}

void closeRom()
{
    FILE *ramFile;
    byte banks;

    // Return if there is no battery backed RAM
    if (!strlen(ramName))
        return;

    // Save battery backed RAM to file
    ramFile = getFileHandle(ramName,"wb", false);
    if (ramFile) {
        banks = getRamBanks();

        for (int i = 0; i < banks; i++) {
            fileWrite(&ram[i][0], sizeof(byte), 0x2000, ramFile);
        }
    }

    closeFileHandle(ramFile);
}
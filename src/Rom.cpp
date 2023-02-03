#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Rom.h"
#include "Mmu.h"
#include "Typedefs.h"
#include "Logger.h"
#include "Misc.h"
#include "Window.h"

char romName[100];
char ramName[100];
char buf[100];
struct RomHeader * romHeader;

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
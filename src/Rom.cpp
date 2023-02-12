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

void RomComponent::EventHandler(SDL_Event *) 
{
    // not implemented
}

void RomComponent::PokeByte(word addr, byte value) 
{
    switch (addr&0xF000)
    {
        // Rom bank 0
        // TODO: add startup rom
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:        
            mbc->PokeByte(addr, value);
            break;
        // Cart ram
        case 0xA000:
        case 0xB000:
            if (mbc->GetRamMode() == RamRtc) {
                *mbc->GetRtcRegister().GetMap() = value;
            } else if (mbc->GetRamMode() == RamEnabled) {
                ram[mbc->GetRamBank()][addr&0x1FFF] = value;
            }
            break;
    }
}

byte RomComponent::PeekByte(word addr) 
{
    switch (addr&0xF000)
    {
        // Rom bank 0
        // TODO: add startup rom
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
            return rom[0][addr];
        // Rom bank 1..n
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
            return rom[mbc->GetRomBank()][addr&0x3FFF];
        // Cart ram
        case 0xA000:
        case 0xB000:
            if (mbc->GetRamMode() == RamRtc) {
                return *mbc->GetRtcRegister().GetMap();
            } else if (mbc->GetRamMode() == RamEnabled) {
                return ram[mbc->GetRamBank()][addr&0x1FFF];
            } else {
                return unmapped;
            }
    }
}

void RomComponent::Cycle() 
{
    // not implemented
}

void RomComponent::Reset() 
{
    // not implemented
}

bool RomComponent::MemoryMapped(word addr)
{
    switch (addr&0xF000)
    {
        case 0x0000:
        case 0x1000:
        case 0x2000:
        case 0x3000:
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:        
        case 0xA000:
        case 0xB000:
            return true;
        default:
            return false;
    }
}

byte RomComponent::GetRamBanks()
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

void RomComponent::Load(const char * rmt)
{
    FILE *romFile;
    FILE *ramFile;
    byte banks;
    
    strcpy(romName, rmt);
    romFile = getFileHandle(romName,"rb", true);

    sprintf(buf, "Rom %s loaded", romName);
    Log(buf, INFO);

    // Read bank 0 and 1 - all carts have these banks
    fileRead(&rom[0][0], sizeof(byte), 0x4000, romFile);
    fileRead(&rom[1][0], sizeof(byte), 0x4000, romFile);

    // Set up rom header
    romHeader = (RomHeader*)(&rom[0][0]+0x104);

    sprintf(buf, "Cart type: %x", romHeader->cartType);
    Log(buf, INFO);

    // Load rom
    switch (romHeader->cartType) {
        case 0:
            mbc = new Mbc0;
            break;
        case 0x01:     // Handle MBC 1
            mbc = new Mbc1;
            break;
        case 0x03:
        case 0x10:     // Handle MBC 3
            mbc = new Mbc3;
            break;
        case 0x13:
        case 0x1b:     // Handle MBC 5
            mbc = new Mbc5;
            break;
        default:
            sprintf(buf, "Unsupported MBC code %x", romHeader->cartType);
            Log(buf, ERROR);
            exit(1);
    }

    // Read all ROM banks
    if (romHeader->cartType != 0) {
        for (int i = 2; i < (0x2 << romHeader->romSize); i++) {
            fileRead(&rom[i][0], sizeof(byte), 0x4000, romFile);
        }
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
                banks = GetRamBanks();
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

void RomComponent::Close()
{
    FILE *ramFile;
    byte banks;

    // Return if there is no battery backed RAM
    if (!strlen(ramName))
        return;

    // Save battery backed RAM to file
    ramFile = getFileHandle(ramName,"wb", false);
    if (ramFile) {
        banks = GetRamBanks();

        for (int i = 0; i < banks; i++) {
            fileWrite(&ram[i][0], sizeof(byte), 0x2000, ramFile);
        }
    }

    closeFileHandle(ramFile);
}
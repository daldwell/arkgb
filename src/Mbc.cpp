#include <algorithm>
#include "Mbc.h"
#include "Logger.h"

byte MbcBase::GetRamBank()
{
    return ram_bnk_no;
}

RamMode MbcBase::GetRamMode()
{
    return ramMode;
}

byte MbcBase::GetRomBank()
{
    return rom_bnk_no;
}

RtcRegister MbcBase::GetRtcRegister()
{
    return rtc;
}

void Mbc0::PokeByte(word addr, byte value)
{
    if (addr <= 0x7FFF) {
        // Block ROM writes
        return;
    }

    Log("Error! Invalid write address MBC0", ERROR);
    exit(1);
}

void Mbc1::PokeByte(word addr, byte value)
{
    // Byte written to ROM space - this updates ROM banking register
    if (addr <= 0x1FFF) {
        // Toggle cart RAM register
        ramMode = (value == 0xA ? RamEnabled : RamDisabled);
        return;
    } else if (addr <= 0x3FFF) {
        // Set ROM bank
        rom_bnk_no = std::max(1, (value&0x1F));
        return;
    } else if (addr <= 0x5FFF) {
         // Set RAM bank
        ram_bnk_no = (value&0x3);
        return;
    } else if (addr <= 0x7FFF) {
        // TODO: handle Banking select mode
        return;
    }

    Log("Error! Invalid write address MBC1", ERROR);
    exit(1);
}

void Mbc3::PokeByte(word addr, byte value)
{
    // Byte written to ROM space - this updates ROM banking register
    if (addr <= 0x1FFF) {
        // Toggle cart RAM register
        ramMode = (value == 0xA ? RamEnabled : RamDisabled);
        return;
    } else if (addr <= 0x3FFF) {
        // Set ROM bank
        rom_bnk_no = std::max(1, (value&0x7F));
        return;
    } else if (addr <= 0x5FFF) {

        // Set RAM bank
        // TODO: handle real time clock
        if (ramMode != RamDisabled) {
            if ((value) >= 0x8 && (value) <= 0xC ) {
                rtc.SetMap(value);
                ramMode = RamRtc;
                return;
            } else if (value <= 0x3) {
                ramMode = RamEnabled;
                ram_bnk_no = (value&0x3);
                return;
            }
        } 
        return;

    } else if (addr <= 0x7FFF) {
        // TODO: check value 0
        if (value == 0x1) {
            rtc.LatchClock();
        }

        return;
    }

    Log("Error! Invalid write address MBC3", ERROR);
    exit(1);
}

void Mbc5::PokeByte(word addr, byte value)
{
    // Byte written to ROM space - this updates ROM banking register
    if (addr <= 0x1FFF) {
        // Toggle cart RAM register
        ramMode = (value == 0xA ? RamEnabled : RamDisabled);
        return;
    } else if (addr <= 0x2FFF) {
        // Set ROM bank
        rom_bnk_no = value;
        return;
    } else if (addr <= 0x3FFF) {
        // TODO: handle 9th bit of ROM bank select
        return;
    } else if (addr <= 0x5FFF) {
        // Set RAM bank
        ram_bnk_no = (value&0xF);
        return;
    } else if (addr <= 0x7FFF) {
        // TODO: handle Banking select mode
        return;
    }

    Log("Error! Invalid write address MBC5", ERROR);
    exit(1);
}
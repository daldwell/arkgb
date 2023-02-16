#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "Typedefs.h"
#include "Cpu.h"
#include "Mmu.h"
#include "Display.h"
#include "Interrupt.h"
#include "Window.h"
#include "GUnit.h"

const int tileWidth = 8;
const int tileHeight = 8;
const int screenWidth = 256;
const int screenHeight = 256;
const int tileIndex = 16;

const int spriteYOffset = 16;
const int spriteXOffset = 8;

const int LCDC_PPU_ENABLE_MASK = 1<<7;
const int LCDC_WINDOW_TILE_MAP_MASK = 1<<6;
const int LCDC_WINDOW_ENABLE_MASK = 1<<5;
const int LCDC_BG_WINDOW_TILE_DATA_MASK = 1<<4;
const int LCDC_BG_TILE_MAP_MASK = 1<<3;
const int LCDC_SPRITE_SIZE_MASK = 1<<2;
const int LCDC_SPRITE_ENABLE_MASK = 1<<1;
const int LCDC_BG_WINDOW_ENABLE_MASK = 0x1;

const int STAT_LYCLC = 1<<6;
const int STAT_MODE2 = 1<<5;
const int STAT_MODE1 = 1<<4;
const int STAT_MODE0 = 1<<3;

const int OAM_BG_WINDOW_OVER_OBJ = 1<<7;
const int OAM_Y_FLIP = 1<<6;
const int OAM_X_FLIP = 1<<5;
const int OAM_PALETTE = 1<<4;

const int BG_OAM_PRIORITY = 1<<7;
const int BG_Y_FLIP = 1<<6;
const int BG_X_FLIP = 1<<5;
const int BG_BANK = 1<<3;
const int BG_PALETTE = 0x7;

const int MODE0_HBLANK = 0;
const int MODE1_VBLANK = 0x1;
const int MODE2_OAMSCAN = 0x2;
const int MODE3_DRAW = 0x3;

const int MODE0_CYCLES = 204;
const int MODE1_CYCLES = 456;
const int MODE2_CYCLES = 80;
const int MODE3_CYCLES = 172;

Pixel bgWinBuffer[160];
Pixel sprBuffer[160];
int dmaCounter = 160;
bool lycEnable;

// DMG palette (this is hardcoded)
RGB dmgPal[4] = {
    {0xFF, 0xFF, 0xFF},
    {0xC0, 0xC0, 0xC0},
    {0x60, 0x60, 0x60},
    {0x00, 0x00, 0x00}
};

const int palSize = 64;
// CGB background palette byte array
bool cgbBGIncrement;
byte cgbBGIndex;
byte cgbBGPal[palSize];

// CGB sprite palette byte array
bool cgbOAMIncrement;
byte cgbOAMIndex;
byte cgbOAMPal[palSize];

void DisplayComponent::EventHandler(SDL_Event * e)
{
    // Not implemented
}

byte DisplayComponent::PeekByte(word addr) 
{

    switch(addr)
    {
        case 0xFF51:
            // High byte
            return vdmaRegs.src>>8;
        case 0xFF52:
            // Low byte
            return vdmaRegs.src&0xFF;
        case 0xFF53:
            // High byte
            return vdmaRegs.dst>>8;
        case 0xFF54:
            // Low byte
            return vdmaRegs.dst&0xFF;
        case 0xFF55:
            return vdmaRegs.init;
    }

    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        return *((byte*)&oamTable + (addr&0xFF));
    }

    if (addr >= 0xFF40 && addr <= 0xFF4B) {
        return *((byte*)&lcdRegs + (addr&0xF));
    }

    // CGB background color index
    if (addr == 0xFF68) {
        return (cgbBGIncrement << 7) + cgbBGIndex;
    }

    // CGB background color palette read
    if (addr == 0xFF69) {
        return cgbBGPal[cgbBGIndex];
    }

    // CGB sprite color index
    if (addr == 0xFF6A) {
        return (cgbOAMIncrement << 7) + cgbOAMIndex;
    }

    // CGB sprite color palette read
    if (addr == 0xFF6B) {
        return cgbOAMPal[cgbOAMIndex];
    }

    // VRam
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        return vram[vram_bnk_no][addr&0x1FFF];
    }

    // Get VRAM bank
    if (addr == 0xFF4F) {
        return 0xFE + vram_bnk_no;
    }
}

void DisplayComponent::PokeByte(word addr, byte value)
{

    switch(addr)
    {
        case 0xFF51:
            // High byte
            vdmaRegs.src = (word)(value<<8) + (vdmaRegs.src&0xF0);
            return;
        case 0xFF52:
            // Low byte
            vdmaRegs.src = (vdmaRegs.src&0xFF00) + (value&0xF0);
            return;
        case 0xFF53:
            // High byte (top 3 bits are ignored)
            vdmaRegs.dst = (word)( (value&0x1F) <<8 ) + (vdmaRegs.dst&0xF0);
            return;
        case 0xFF54:
            // Low byte
            vdmaRegs.dst = (vdmaRegs.dst&0x1F00) + (value&0xF0);
            return;
        case 0xFF55:
            // Commence HDMA transfer
            vdmaRegs.init = value&0x7f; // Store lower bits, higher bit is used to check DMA type, then discarded

            // Stop/restart HDMA transfer if active
            if (vdmaRegs.status == HBL || vdmaRegs.status == HPS) {
                if (! (value&0x80) ) {
                    vdmaRegs.init |= 0x80; // Stop copy
                    vdmaRegs.status = OFF;
                }
                return;
            }

            // Check high bit to enable General/Hblank DMA
            if (value&0x80) {
                vdmaRegs.status = ( (lcdRegs.STAT & 0x3) == MODE0_HBLANK ) ? HBL : HPS; // Start off paused, will awaken before next Hblank
            } else {
                vdmaRegs.status = GEN;
            }

            return;
    }

    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        *((byte*)&oamTable + (addr&0xFF)) = value;
        return;
    }

    // CGB background color index
    if (addr == 0xFF68) {
        cgbBGIndex = value & 0x3F;
        cgbBGIncrement = value & 0x80;
        return;
    }

    // CGB background color palette write
    if (addr == 0xFF69) {
        cgbBGPal[cgbBGIncrement ? cgbBGIndex++ : cgbBGIndex] = value;
        return;
    }

    // CGB sprite color index
    if (addr == 0xFF6A) {
        cgbOAMIndex = value & 0x3F;
        cgbOAMIncrement = value & 0x80;
        return;
    }

    // CGB sprite color palette write
    if (addr == 0xFF6B) {
        cgbOAMPal[cgbOAMIncrement ? cgbOAMIndex++ : cgbOAMIndex] = value;
        return;
    }

    // VRam
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        vram[vram_bnk_no][addr&0x1FFF] = value;
        return;
    }

    // Set VRAM bank
    if (addr == 0xFF4F) {
        vram_bnk_no = (value & 0x1);
        return;
    }

    // Clear screen if ppu is disabled
    if (addr == 0xFF40 && !(value&LCDC_PPU_ENABLE_MASK)) {
        gwindow.ClearSurface();
        gwindow.RefreshWindow();
    }

    *((byte*)&lcdRegs + (addr&0xF)) = value;
}

int DisplayComponent::GetColorFromPalette(RGB rgb)
{
    return gwindow.GetRGB(rgb.r, rgb.g, rgb.b);
}

void DisplayComponent::DrawBackgroundRow(byte y)
{
    int tileX; // X coordinate of tile, between 0-31
    int tileY; // Y coordinate of tile, between 0-255
    bool addressMode =  lcdRegs.LCDC & LCDC_BG_WINDOW_TILE_DATA_MASK;
    int tileMapAddr;
    int tileDataAddr = 0x8000 + (addressMode ? 0x0 : 0x800);
    word tileData;
    bool cgbProfile = (profile == CGB);
    byte cIdx;
    int color;
    byte palIndex;
    word palData;
    int startingXPixel;

    // Init background row buffer
    for (int i = 0; i < 160; i++) {
        bgWinBuffer[i].cIdx = 0x0;
        bgWinBuffer[i].rgb = dmgPal[0];
        bgWinBuffer[i].priority = 0;
        bgWinBuffer[i].BGWinOverOAM = false;
    }

    // Exit if window/background not enabled
    if (!(lcdRegs.LCDC & LCDC_BG_WINDOW_ENABLE_MASK)) return;

    int x = 0;
    while (x < 160) {

        // Are we drawing the window?
        bool windowTile = (lcdRegs.LCDC & LCDC_WINDOW_ENABLE_MASK) && (x >= (lcdRegs.WX-0x7) && y >= lcdRegs.WY);

        // Configure bg/window values
        if (windowTile) {
            tileX = ( (x-(lcdRegs.WX-0x7))/8 ) & 0x1F;
            tileY = (y-lcdRegs.WY) & 0xFF;
            tileMapAddr = (lcdRegs.LCDC & LCDC_WINDOW_TILE_MAP_MASK) ? 0x9C00 : 0x9800;
            startingXPixel = 0;
        } else {
            tileX = ((lcdRegs.SCX + x)/8) & 0x1F;
            tileY = (lcdRegs.SCY + y) & 0xFF;
            tileMapAddr = (lcdRegs.LCDC & LCDC_BG_TILE_MAP_MASK) ? 0x9C00 : 0x9800;
            startingXPixel = (x > 0) ? 0 : (lcdRegs.SCX & 0x7);
        }

        // Get tile map
        // Set to bank 0 for tile offset
        int tileMapOffset = tileMapAddr + tileX + ((tileY/8) << 5);
        mmu.PokeByte(0xFF4F, 0x0);

        // Index to tile data
        byte tIdx = ( (sbyte)mmu.PeekByte(tileMapOffset) + (addressMode ? 0 : 0x80) );

        // Get CGB attributes
        // Requires a bank switch
        mmu.PokeByte(0xFF4F, 0x1);
        byte tAttr = mmu.PeekByte(tileMapOffset);

        // Calculate tile Y offset, taking into account vertical flip bit (CGB only)
        byte tileRow = tileY & 0x7;
        if ((tAttr & BG_Y_FLIP) && cgbProfile ) tileRow = ((tileHeight-1) - tileRow);
        tileRow <<= 1; // Multiply offset as tiles are 2 bytes long

        // CGB requires a bank switch for the correct tile data
        byte sprBnk = (cgbProfile && (tAttr & 0x8)) ? 1 : 0; 
        mmu.PokeByte(0xFF4F, sprBnk);

        // Get the tile pixel data
        tileData = mmu.PeekWord(tileDataAddr + (tIdx << 4) + tileRow);

        for (int k = 7-startingXPixel; k >= 0; k--) {
            // Horizontal flip - read bits from other direction (CGB only)
            int tileBit = ((tAttr&BG_X_FLIP) && cgbProfile) ? (tileWidth-1) - k : k;
            
            cIdx = tileData & (0x100<<tileBit) ? 2 : 0;
            cIdx += tileData & (0x1<<tileBit) ? 1 : 0;

            if (cgbProfile) {
                palIndex = (tAttr & 0x7) << 3;
                palData = cgbBGPal[palIndex + (cIdx<<1) + 1] << 8;
                palData += cgbBGPal[palIndex + (cIdx<<1)];

                bgWinBuffer[x].cIdx = cIdx;
                bgWinBuffer[x].rgb.r = (palData & 0x1F) << 3;
                bgWinBuffer[x].rgb.g = ((palData>>5) & 0x1F) << 3;
                bgWinBuffer[x].rgb.b = ((palData>>10) & 0x1F) << 3;
                bgWinBuffer[x].BGWinOverOAM = tAttr&BG_OAM_PRIORITY;
            } else {
                bgWinBuffer[x].cIdx = cIdx;
                cIdx<<=1;
                bgWinBuffer[x].rgb = dmgPal[(lcdRegs.BGP>>cIdx & 0x3)];
                bgWinBuffer[x].BGWinOverOAM = false;  // flag does not exist for DMG
            }
            x++;
        }
    }
}

void DisplayComponent::DrawSpritesRow(byte y)
{
    Sprite spr;
    word tileRow;
    byte cIdx;
    int color;
    byte tileYOffset;
    int rx = 0;
    word bp = 0x8000;
    bool hasPriority;
    byte palIndex;
    word palData;
    byte tileSize = (lcdRegs.LCDC&LCDC_SPRITE_SIZE_MASK) ? 15 : 7;
    bool cgbProfile = (profile == CGB);

    // Init sprite pixel buffer
    for (int i = 0; i < 160; i++) {
        sprBuffer[i].cIdx = 0x0;
        sprBuffer[i].rgb = dmgPal[0];
        sprBuffer[i].priority = 0xFFF;
        sprBuffer[i].BGWinOverOAM = false;
    }

    for (int i = 0; i < 40; i++) {
        spr = oamTable[i];
        hasPriority = false;
        int xPos = spr.xPos - spriteXOffset;
        int yPos = spr.yPos - spriteYOffset;

        // If sprite Y offset is outside the screen, skip
        if (yPos <= ((tileSize+1)*-1) || yPos >= 168) continue;

        // Skip sprite if it falls outside the current scan line
        if (y < yPos || y > yPos+tileSize) continue;

        tileYOffset = (spr.attr&OAM_Y_FLIP ? ( tileSize - (y - yPos) ) : (y - yPos)) * 2;

        // CGB requires a bank switch for the correct tile data
        byte sprBnk = (cgbProfile && (spr.attr & 0x8)) ? 1 : 0; 
        mmu.PokeByte(0xFF4F, sprBnk);

        tileRow = mmu.PeekWord(bp + tileYOffset + (spr.tIdx*tileIndex));
        rx = xPos-1;
        for (int k = 7; k >= 0; k--) {

            rx ++;

            // Sprite pixel outside scanline
            if (rx < 0 || rx >= 160) {
                continue;
            }

            // Horizontal flip - read bits from other direction
            int tileBit = (spr.attr&OAM_X_FLIP) ? (tileWidth-1) - k : k;

            cIdx = tileRow & (0x100<<tileBit) ? 2 : 0;
            cIdx += tileRow & (0x1<<tileBit) ? 1 : 0;

            // Transparent pixel
            if (cIdx == 0) {
                continue;
            }

            if (cgbProfile) {
                // Priority check
                if ( (sprBuffer[rx].priority <= i ) ) {
                    continue;
                }

                palIndex = (spr.attr & 0x7) << 3;
                palData = cgbOAMPal[palIndex + (cIdx<<1) + 1] << 8;
                palData += cgbOAMPal[palIndex + (cIdx<<1)];

                sprBuffer[rx].cIdx = cIdx;
                sprBuffer[rx].rgb.r = (palData & 0x1F) << 3;
                sprBuffer[rx].rgb.g = ((palData>>5) & 0x1F) << 3;
                sprBuffer[rx].rgb.b = ((palData>>10) & 0x1F) << 3;
                sprBuffer[rx].priority = i;
                sprBuffer[rx].BGWinOverOAM = spr.attr&OAM_BG_WINDOW_OVER_OBJ;

            } else {
                // Priority check
                if ( (sprBuffer[rx].priority <= spr.xPos ) ) {
                    continue;
                }

                sprBuffer[rx].cIdx = cIdx;
                cIdx<<=1;
                sprBuffer[rx].rgb = dmgPal[(((spr.attr&OAM_PALETTE) ? lcdRegs.OBP1 : lcdRegs.OBP0 ) >>cIdx & 0x3)];
                sprBuffer[rx].priority = spr.xPos;
                sprBuffer[rx].BGWinOverOAM = spr.attr&OAM_BG_WINDOW_OVER_OBJ;
            }

        }        
    }
}

void DisplayComponent::RenderFrame(byte y)
{
    int color;

    // Merge the BG/Sprite pixels together
    for (int i = 0; i < 160; i++) {

        Pixel bgPixel = bgWinBuffer[i];
        Pixel oamPixel = sprBuffer[i];

        //Resolve sprite vs background pixel priority
        if ( oamPixel.cIdx != 0 && ( bgPixel.cIdx == 0 || ( !bgPixel.BGWinOverOAM && !oamPixel.BGWinOverOAM ) ) ) {
            color = GetColorFromPalette(oamPixel.rgb);
        } else {
            color = GetColorFromPalette(bgPixel.rgb);
        }

        gwindow.DrawPixel(i, y, color);
    }    
}

void DisplayComponent::PerformVDMA()
{
    // We do this every other cycle for doublespeed mode as VDMA is tied to the base clock rate
    vdmaRegs.cycles += (4 >> doubleSpeed);
    if (vdmaRegs.cycles != 4) {
        return;
    }
    vdmaRegs.cycles = 0;
    
    // Skip if not active, or HDMA has paused for the HBlank period
    if (vdmaRegs.status == OFF || vdmaRegs.status == HPS) {
        return;
    }

    halt = true; // CPU is halted for duration of transfer

    mmu.PokeByte((vdmaRegs.dst+0x8000) + (vdmaRegs.count), mmu.PeekByte(vdmaRegs.src + (vdmaRegs.count)));

    // Increment the count, do not make further adjustments until 16 bytes have been transferred.
    vdmaRegs.count++;
    if (vdmaRegs.count % 0x10) {
        return;
    } 

    // No more bytes left - VDMA Transfer complete
    if (vdmaRegs.init == 0) {
        vdmaRegs.status = OFF;
    }

    // Adjust counts if we are still going
    if (vdmaRegs.status != OFF) {

        // Decrement master count
        vdmaRegs.init--;

        // Pause hdma - only 16 bytes are transferred during a single hblank period
        if (vdmaRegs.status == HBL) {
            halt = false;
            vdmaRegs.status = HPS;
        }

    } else {
        // All done, reset VDMA values
        halt = false;
        vdmaRegs.init = 0xFF;
        vdmaRegs.count = 0;
    }
}

void DisplayComponent::Cycle()
{
    int modeCycles = 0;
    word dmaSrc = (word)lcdRegs.DMA << 8;
    //int cycles = cpuCycles;

    for (int cycles = 4; cycles <= cpuCycles; cycles += 4)
    {
        // Perform VRAM DMA transfer if register is set
        PerformVDMA();
        
        // Perform OAM DMA transfer if register is set
        if (dmaSrc && dmaCounter == 160) {
            dmaCounter = 0;
        }

        if (dmaCounter < 160) {
            mmu.PokeByte(0xFE00 + dmaCounter, mmu.PeekByte(dmaSrc+dmaCounter));
            dmaCounter++;
            if (dmaCounter == 160) {
                lcdRegs.DMA = 0;
                dmaSrc = 0;
            }
        }

        // Exit if LCD is disabled
        if (!(lcdRegs.LCDC&LCDC_PPU_ENABLE_MASK)) {
            continue;
        }

        displayCycles += 4;

        switch (lcdRegs.STAT & 0x3)
        {
            //TODO: enums and define mode flag masks
            case MODE0_HBLANK:
                modeCycles = MODE0_CYCLES << doubleSpeed;
                // Execute HDMA cycle here if flag is enabled
                //PerformVDMA();

                if (displayCycles >= modeCycles) {
                    displayCycles -= modeCycles;
                    lcdRegs.LY++;

                    if (lcdRegs.LY == lcdRegs.LYC)
                        lycEnable = true;

                    if (lcdRegs.LY > 143) {

                        // TODO: drawing routines
                        gwindow.RefreshWindow();
                        gwindow.ClearSurface();

                        // Turn on vblank mode and enable vblank interrupt
                        lcdRegs.STAT &= ~(MODE3_DRAW);
                        lcdRegs.STAT |= MODE1_VBLANK;
                        IFRegister |= vblank_flag;

                        // Check if we can enable vblank stat interrupt 
                        if (lcdRegs.STAT & STAT_MODE1) {
                            IFRegister |= lcd_flag;
                        }

                    } else {
                        lcdRegs.STAT &= ~(MODE3_DRAW);
                        lcdRegs.STAT |= MODE2_OAMSCAN;

                        // Check if we can enable OAM interrupt 
                        if (lcdRegs.STAT & STAT_MODE2) {
                            IFRegister |= lcd_flag;
                        }
                    }
                }
                break;
            case MODE2_OAMSCAN:
                modeCycles = MODE2_CYCLES << doubleSpeed;
                if (displayCycles >= modeCycles) {
                    displayCycles -= modeCycles;
                    lcdRegs.STAT &= ~(MODE3_DRAW);
                    lcdRegs.STAT |= MODE3_DRAW;
                }
                break;
            case MODE3_DRAW:
                modeCycles = MODE3_CYCLES << doubleSpeed;
                if (displayCycles >= modeCycles) {

                    // Vram bank is trashed during screen refresh
                    byte tmp_vram_bnk = mmu.PeekByte(0xFF4F);

                    DrawBackgroundRow(lcdRegs.LY);
                    DrawSpritesRow(lcdRegs.LY);
                    RenderFrame(lcdRegs.LY);
                    displayCycles -= modeCycles;
                    lcdRegs.STAT &= ~(MODE3_DRAW);

                    // Restore original value when done
                    mmu.PokeByte(0xFF4F, tmp_vram_bnk);

                    // Check if we can enable hblank stat interrupt 
                    if (lcdRegs.STAT & STAT_MODE0) {
                        IFRegister |= lcd_flag;
                    }

                    // Enable HDMA if flag is paused
                    if (vdmaRegs.status == HPS) {
                        vdmaRegs.status = HBL;
                    }
                }
                break;
            case MODE1_VBLANK:
                modeCycles = MODE1_CYCLES << doubleSpeed;
                if (displayCycles >= modeCycles) {
                    displayCycles -= modeCycles;

                    if (lcdRegs.LY == 0) {
                        //displayCycles = 0;
                        lcdRegs.STAT &= ~(MODE3_DRAW);
                        lcdRegs.STAT |= MODE2_OAMSCAN;
                        lcdRegs.LY = 0;

                        // Check if we can enable OAM interrupt 
                        if (lcdRegs.STAT & STAT_MODE2) {
                            IFRegister |= lcd_flag;
                        }

                        // Enable HDMA if flag is paused
                        if (vdmaRegs.status == HPS) {
                            vdmaRegs.status = HBL;
                        }

                        break;
                    }

                    lcdRegs.LY++;

                    // LY gets treated as zero at 153 (apparently an undocumented quirk)
                    if (lcdRegs.LY == 153) {
                        lcdRegs.LY = 0;
                        if (lcdRegs.LY == lcdRegs.LYC)
                        lycEnable = true;
                    }
                }

                break;
        }

        // Check if we can enable LYC:LY interrupt 
        if ((lcdRegs.STAT & STAT_LYCLC) && lycEnable) {
            IFRegister |= lcd_flag;
            lycEnable = false;
        }
    }

}

bool DisplayComponent::MemoryMapped(word addr)
{
    // HDMA registers
    switch(addr)
    {
        case 0xFF51:
        case 0xFF52:
        case 0xFF53:
        case 0xFF54:
        case 0xFF55:
            return true;
    }

    // OAM table
    if (addr >= 0xFE00 && addr <= 0xFE9F) {
        return true;
    }

    // LCD registers
    if (addr >= 0xFF40 && addr <= 0xFF4B) {
        return true;
    }

    // CGB background color palette 
    if (addr == 0xFF68 || addr == 0xFF69) {
        return true;
    }

    // CGB sprite color palette 
    if (addr == 0xFF6A || addr == 0xFF6B) {
        return true;
    }

    // VRam
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        return true;
    }

    // VRAM bank
    if (addr == 0xFF4F) {
        return true;
    }

    return false;
}

void DisplayComponent::Reset()
{
    displayCycles = 200;

    // Display regs
    lcdRegs.LCDC = 0x91; 
    lcdRegs.STAT = 0x85;
    lcdRegs.SCY = 0x0;
    lcdRegs.SCX = 0x0;
    lcdRegs.LY = 0x91;
    lcdRegs.LYC = 0x0;
    lcdRegs.DMA = 0xFF;
    lcdRegs.BGP = 0xFC;
    lcdRegs.OBP0 = 0x0;
    lcdRegs.OBP1 = 0x0;
    lcdRegs.WX = 0x0;
    lcdRegs.WY = 0x0;

    // hmda regs
    vdmaRegs.init = 0xFF;
    vdmaRegs.status = OFF;
    vdmaRegs.count = 0;
}

VdmaStatus DisplayComponent::GetVdmaStatus()
{
    return vdmaRegs.status;
}
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

Sprite oamTable[40];
struct LcdRegister lcdRegs;
int displayCycles;
byte bgWinBuffer[256];
int sprBuffer[256];
int dmaCounter = 160;
bool lycEnable;

// DMG palette (this is hardcoded)
Pixel dmgPal[4] = {
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

int DisplayComponent::GetColorFromPalette(Pixel px)
{
    return gwindow.GetRGB(px.r, px.g, px.b);
}

void DisplayComponent::DrawTileRow(byte tIdx, int x, int y, byte tileYOffset, byte iniPxl, word bpOffset, byte tAttr)
{
    word tileRow;
    byte cIdx;
    int color;
    int rx = 0;
    Pixel px;
    byte palIndex;
    word palData;
    word bp = 0x8000 + bpOffset;
    bool cgbProfile = (profile == CGB);

    // CGB requires a bank switch for the correct tile data
    byte sprBnk = (cgbProfile) ? (tAttr & 0x8) : 0; 
    mmu.PokeByte(0xFF4F, sprBnk);

    tileRow = mmu.PeekWord(bp + tileYOffset + (tIdx*tileIndex));
    rx = x;
    for (int k = 7-iniPxl; k >= 0; k--) {
        cIdx = tileRow & (0x100<<k) ? 2 : 0;
        cIdx += tileRow & (0x1<<k) ? 1 : 0;

        if (cgbProfile) {
            palIndex = (tAttr & 0x7) << 3;
            palData = cgbBGPal[palIndex + (cIdx<<1) + 1] << 8;
            palData += cgbBGPal[palIndex + (cIdx<<1)];

            px.r = (palData & 0x1F) << 3;
            px.g = ((palData>>5) & 0x1F) << 3;
            px.b = ((palData>>10) & 0x1F) << 3;
        } else {
            cIdx<<=1;
            px = dmgPal[(lcdRegs.BGP>>cIdx & 0x3)];
        }

        color = GetColorFromPalette(px);
        gwindow.DrawPixel(rx, y, color);
        // Keep track of BG/Win pixels, used to mask sprites later
        // Colour index 0 is not tracked
        if (!bgWinBuffer[rx] && cIdx) bgWinBuffer[rx] = cIdx;
        rx ++;
    }
}

void DisplayComponent::DrawSpritesRow(byte y)
{
    Sprite spr;
    word tileRow;
    byte cIdx;
    int color;
    byte tileYOffset;
    byte pixelXPos;
    int rx = 0;
    word bp = 0x8000;
    bool hasPriority;
    Pixel px;
    byte palIndex;
    word palData;
    byte tileSize = (lcdRegs.LCDC&LCDC_SPRITE_SIZE_MASK) ? 15 : 7;
    bool cgbProfile = (profile == CGB);

    // For the sprite buffer we use two masking values:
    // 0xFF means no pixel is occupied at all by a sprite
    // 0xFE means a transparent sprite pixel. NOTE - any sprite attempting to draw over a "background" sprite that has priority will also have their pixels masked.
    for (int i = 0; i < 256; i++) {
        sprBuffer[i] = 0xF;
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
        byte sprBnk = cgbProfile ? (spr.attr & 0x8) : 0; 
        mmu.PokeByte(0xFF4F, sprBnk);

        tileRow = mmu.PeekWord(bp + tileYOffset + (spr.tIdx*tileIndex));
        rx = xPos;
        for (int k = 7; k >= 0; k--) {
            cIdx = tileRow & (0x100<<k) ? 2 : 0;
            cIdx += tileRow & (0x1<<k) ? 1 : 0;

            if (cgbProfile) {
                palIndex = (spr.attr & 0x7) << 3;
                palData = cgbOAMPal[palIndex + (cIdx<<1) + 1] << 8;
                palData += cgbOAMPal[palIndex + (cIdx<<1)];

                px.r = (palData & 0x1F) << 3;
                px.g = ((palData>>5) & 0x1F) << 3;
                px.b = ((palData>>10) & 0x1F) << 3;
            } else {
                cIdx<<=1;
                px = dmgPal[(((spr.attr&OAM_PALETTE) ? lcdRegs.OBP1 : lcdRegs.OBP0 ) >>cIdx & 0x3)];
            }
            color = GetColorFromPalette(px);

            // Determine pixel position based on sprite flip attribute
            pixelXPos = spr.attr&OAM_X_FLIP ? (xPos+7) - (rx-xPos): rx;

            // Sprite is hidden behind background
            if (spr.attr&OAM_BG_WINDOW_OVER_OBJ && bgWinBuffer[pixelXPos]) {
                color = 0xE;
            }

            // Check if the sprite has priority for this pixel position
            // The leftmost sprite has priority over others
            // If two sprites share the same position, the first one in OAM memory has priority
            if (sprBuffer[pixelXPos] == 0xF || hasPriority) {
                hasPriority = true;
                if (cIdx != 0) sprBuffer[pixelXPos] = color;
            }

            rx ++;
        }        
    }

    for (int i = 0; i < 256; i++) {
        if (sprBuffer[i] != 0xE && sprBuffer[i] != 0xF) gwindow.DrawPixel(i, y, sprBuffer[i]);
    }
}

void DisplayComponent::DrawBackgroundRow(byte y)
{
    byte tIdx;
    byte sx = 0;
    byte sy = 0;
    int x = 0;
    //int y = 0;
    bool addressMode = false;

    // Exit if window/background not enabled
    if (! lcdRegs.LCDC & LCDC_BG_WINDOW_ENABLE_MASK) return;

    // Init background row buffer
    for (int i = 0; i < 256; i++) {
        bgWinBuffer[i] = 0x0;
    }

    // Get tile data addressing mode
    addressMode =  lcdRegs.LCDC & LCDC_BG_WINDOW_TILE_DATA_MASK;

    // Get tile map memory location
    word tileMapBase = (lcdRegs.LCDC & LCDC_BG_TILE_MAP_MASK) ? 0x9C00 : 0x9800;

    // Starting pixel for first tile - offset from current X scroll register value
    word startingXPixel = lcdRegs.SCX % 8;

    // Draw a single horizontal (x) row across the screen
    word tilesRow = (((byte)(lcdRegs.SCY + y) / tileHeight) * 32);

    byte scrollOffset = lcdRegs.SCX / 8;

    // Set to bank 0 for tile offset
    mmu.PokeByte(0xFF4F, 0x0);

    for (int i = 0; i < 32; i++) {
        sbyte tIdx = mmu.PeekByte(tileMapBase + tilesRow + (i + scrollOffset) % 32);
        byte tAttr = 0;

        // Get CGB attributes
        // Requires a bank switch
        mmu.PokeByte(0xFF4F, 0x1);
        tAttr = mmu.PeekByte(tileMapBase + tilesRow + (i + scrollOffset) % 32);

        // Draw the tiles
        DrawTileRow(addressMode ? (byte)tIdx : tIdx+0x80, x, y, (((lcdRegs.SCY%8) + y ) % tileWidth) * 2, i == 0 ? startingXPixel : 0, addressMode ? 0x0 : 0x800, tAttr);
        x += (tileWidth - (i == 0 ? startingXPixel : 0));
    }      
}

void DisplayComponent::DrawWindowRow(byte y)
{
    byte tIdx;
    byte sx = 0;
    byte sy = 0;
    int x = 0;
    bool addressMode = false;

    // Exit if window/background not enabled
    if (!(lcdRegs.LCDC & LCDC_BG_WINDOW_ENABLE_MASK) ||  !(lcdRegs.LCDC & LCDC_WINDOW_ENABLE_MASK)) return;

    // Exit if the scanline is not within the window
    if (y < lcdRegs.WY) return;

    // Get tile data addressing mode
    addressMode =  lcdRegs.LCDC & LCDC_BG_WINDOW_TILE_DATA_MASK;

    // Get tile map memory location
    word tileMapBase = (lcdRegs.LCDC & LCDC_WINDOW_TILE_MAP_MASK) ? 0x9C00 : 0x9800;

    // Draw a single horizontal (x) row across the screen
    word tilesRow = (((y-lcdRegs.WY) / tileHeight) * 32);

    // Set to bank 0 for tile offset
    mmu.PokeByte(0xFF4F, 0x0);

    for (int i = 0; i < 32; i++) {
        sbyte tIdx = mmu.PeekByte(tileMapBase + tilesRow + (i) % 32);

        // Get CGB attributes
        // Requires a bank switch
        mmu.PokeByte(0xFF4F, 0x1);
        byte tAttr = mmu.PeekByte(tileMapBase + tilesRow + (i) % 32);

        // Draw the tiles
        DrawTileRow(addressMode ? (byte)tIdx : tIdx+0x80, x + (lcdRegs.WX-0x7), y, ((y-lcdRegs.WY) % tileWidth) * 2, 0, addressMode ? 0x0 : 0x800, tAttr);
        x += tileWidth;
    }
}

void DisplayComponent::Cycle()
{
    word dmaSrc = (word)lcdRegs.DMA << 8;
    int cycles = cpuCycles >> doubleSpeed;

    // Exit if LCD is disabled
    if (!(lcdRegs.LCDC&LCDC_PPU_ENABLE_MASK)) {
        return;
    }

    // Perform DMA transfer if register is set
    if (dmaSrc && dmaCounter == 160) {
        dmaCounter = 0;
    }

    if (dmaCounter < 160) {
        mmu.PokeByte(0xFE00 + dmaCounter, mmu.PeekByte(dmaSrc+dmaCounter));
        dmaCounter++;
        if (dmaCounter == 160) lcdRegs.DMA = 0;
    }

    displayCycles += cycles;

    switch (lcdRegs.STAT & 0x3)
    {
        //TODO: enums and define mode flag masks
        case 0:
            if (displayCycles >= 204) {
                displayCycles -= 204;
                lcdRegs.LY++;

                if (lcdRegs.LY == lcdRegs.LYC)
                    lycEnable = true;

                if (lcdRegs.LY > 143) {

                    // TODO: drawing routines
                    gwindow.RefreshWindow();
                    gwindow.ClearSurface();

                    // Turn on vblank mode and enable vblank interrupt
                    lcdRegs.STAT &= ~(0x3);
                    lcdRegs.STAT |= 0x1;
                    IFRegister |= vblank_flag;

                    // Check if we can enable vblank stat interrupt 
                    if (lcdRegs.STAT & STAT_MODE1) {
                        IFRegister |= lcd_flag;
                    }

                } else {
                    lcdRegs.STAT &= ~(0x3);
                    lcdRegs.STAT |= 0x2;

                    // Check if we can enable OAM interrupt 
                    if (lcdRegs.STAT & STAT_MODE2) {
                        IFRegister |= lcd_flag;
                    }
                }
            }
            break;
        case 2:
            if (displayCycles >= 80) {
                displayCycles -= 80;
                lcdRegs.STAT &= ~(0x3);
                lcdRegs.STAT |= 0x3;
            }
            break;
        case 3:
            if (displayCycles >= 172) {

                // Vram bank is trashed during screen refresh
                byte tmp_vram_bnk = mmu.PeekByte(0xFF4F);

                DrawBackgroundRow(lcdRegs.LY);
                DrawWindowRow(lcdRegs.LY);
                DrawSpritesRow(lcdRegs.LY);
                displayCycles -= 172;
                lcdRegs.STAT &= ~(0x3);

                // Restore original value when done
                mmu.PokeByte(0xFF4F, tmp_vram_bnk);

                // Check if we can enable hblank stat interrupt 
                if (lcdRegs.STAT & STAT_MODE0) {
                    IFRegister |= lcd_flag;
                }
            }
            break;
        case 1:
            if (displayCycles >= 456) {
                displayCycles -= 456;

                if (lcdRegs.LY == 0) {
                    //displayCycles = 0;
                    lcdRegs.STAT &= ~(0x3);
                    lcdRegs.STAT |= 0x2;
                    lcdRegs.LY = 0;

                    // Check if we can enable OAM interrupt 
                    if (lcdRegs.STAT & STAT_MODE2) {
                        IFRegister |= lcd_flag;
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

bool DisplayComponent::MemoryMapped(word addr)
{
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
    // Not implemented
}

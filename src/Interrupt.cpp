#include <stdlib.h>
#include <assert.h>
#include "Typedefs.h"
#include "Opc.h"
#include "Cpu.h"

byte IERegister;
byte IFRegister;
int IMECounter;
bool IMERegister;

static void interruptServiceRoutine(int a)
{
    PUSH(&regs.PC);
    regs.PC = a;
    cpuCycles += 20;
}

void InterruptComponent::EventHandler(SDL_Event *)
{
    // not implemented
}

byte InterruptComponent::PeekByte(word addr)
{
    if (addr == 0xFF0F) {
        return IFRegister;
    } else if (addr == 0xFFFF) {
        return IERegister;
    }
    // TODO: Log error
}

void InterruptComponent::PokeByte(word addr, byte value) 
{
    if (addr == 0xFF0F) {
        IFRegister = value;
        
    } else if (addr == 0xFFFF) {
        IERegister = value;
    }
    // TODO: Log error
}

bool InterruptComponent::MemoryMapped(word addr) 
{
    if (addr == 0xFF0F || addr == 0xFFFF) {
        return true;
    }
    return false;
}


void InterruptComponent::Cycle()
{
    // If master interrupt register is enabled OR an interrupt is pending, exit halt
    // NOTE - interrupts cannot be triggered during a VDMA transfer
    if (display.GetVdmaStatus() == OFF && (IMERegister || IFRegister)) {
        halt = false;
    }

    if (IMERegister && IERegister && IFRegister) {
        byte fired = IERegister & IFRegister;

        // Vblank interrupt
        if (fired & vblank_flag) {
            IMERegister = false;
            IFRegister &= ~(vblank_flag);
            interruptServiceRoutine(0x40);
            return;
        }

        // LCD stat interrupt
        if (fired & lcd_flag) {
            IMERegister = false;
            IFRegister &= ~(lcd_flag);
            interruptServiceRoutine(0x48);
            return;
        }

        // Timer interrupt
        if (fired & timer_flag) {
            IMERegister = false;
            IFRegister &= ~(timer_flag);
            interruptServiceRoutine(0x50);
            return;
        }

        // Serial interrupt
        if (fired & serial_flag) {
            IMERegister = false;
            IFRegister &= ~(serial_flag);
            interruptServiceRoutine(0x58);
            return;
        }

        // Joypad interrupt
        if (fired & joypad_flag) {
            IMERegister = false;
            IFRegister &= ~(joypad_flag);
            interruptServiceRoutine(0x60);
            return;
        }
    }
}

void InterruptComponent::Reset() 
{
    // Not implemented
}
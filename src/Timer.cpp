#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Timer.h"
#include "Interrupt.h"
#include "Cpu.h"

const int TAC_timerEnable = 0x4;
const int TAC_clockSelect = 0x3;

struct TimerRegister timeRegs;
struct TAC tacs[4]
{
    {0x0200, true},
    {0x0008, true},
    {0x0020, true},
    {0x0080, true}
};
bool timaOver = false;

void TimerComponent::EventHandler(SDL_Event * e)
{
    // Not implemented
}

byte TimerComponent::PeekByte(word addr)
{
    // Not implemented
}

void TimerComponent::PokeByte(word addr, byte value) 
{
    // Not implemented
}

bool TimerComponent::MemoryMapped(word addr) 
{
    // Not implemented
    return false;
}


void TimerComponent::Cycle()
{
    bool incTima = false;
    bool out = false;
    int clockSelect = timeRegs.TAC & TAC_clockSelect;

    // DIV timer goes up at fixed rate 16384hz
    for (int i = 0; i < (cpuCycles/4); i++) {

        // If there is a scheduled TIMA reset, handle now
        if (timaOver) {
            timaOver = false;
            timeRegs.TIMA = timeRegs.TMA;
            // 10/11/2022 FIXED (problem went away after fixing up F register to store flag values): 
            // Original comment: turning off timer interrupt for now as it crashes SML randomly due to apparent memory corruption. Will try re-enable it once more bugs are ironed out elsewhere.
            IFRegister |= timer_flag;
        }

        timeRegs.DIV += 4;

        out = (timeRegs.DIV & tacs[clockSelect].mask);
        if (!out && tacs[clockSelect].edge) {
            tacs[clockSelect].edge = false;
            incTima = true;
        } else if (out) {
            tacs[clockSelect].edge = true;
        }

        // Increment timer if DIV falling edge AND timer enabled
        if (timeRegs.TAC&TAC_timerEnable && incTima) {
            incTima = false;

            // Handle overflow condition - reset timer to timer Modulo register and turn on interrupt
            // This action is delayed by 1 cycle
            if (timeRegs.TIMA == 0xFF) {
                timeRegs.TIMA = 0x0;
                timaOver = true;
            } else {
                timeRegs.TIMA++;
            }
        }
    }
}

void TimerComponent::Reset()
{
    // Not implemented
}
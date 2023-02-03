#include <stdio.h>
#include <assert.h>
#include "Cpu.h"
#include "Typedefs.h"

struct Registers regs;
//struct Flags flags;

#define flag_c_mask 1<<4
#define flag_h_mask 1<<5
#define flag_n_mask 1<<6
#define flag_z_mask 1<<7

bool getFlag(Flags f) {
    switch (f)
    {
        case CF:
            return regs.F & flag_c_mask;
        case HF:
            return regs.F & flag_h_mask;
        case NF:
            return regs.F & flag_n_mask;
        case ZF:
            return regs.F & flag_z_mask;
        default:
            printf("Error: invalid flag to get!");
            exit(1);
    }

}

void setFlag(Flags f, bool toggle) {
    if (toggle) {
        switch (f)
        {
            case CF:
                regs.F |= flag_c_mask;
                break;
            case HF:
                regs.F |= flag_h_mask;
                break;
            case NF:
                regs.F |= flag_n_mask;
                break;
            case ZF:
                regs.F |= flag_z_mask;
                break;
            default:
                printf("Error: invalid flag to set!");
                exit(1);
        }
    } else {
        switch (f)
        {
            case CF:
                regs.F &= ~(flag_c_mask);
                break;
            case HF:
                regs.F &= ~(flag_h_mask);
                break;
            case NF:
                regs.F &= ~(flag_n_mask);
                break;
            case ZF:
                regs.F &= ~(flag_z_mask);
                break;
            default:
                printf("Error: invalid flag to set!");
                exit(1);
        }
    }
}


// void registerCycle()
// {
//     // Emulate real flag register F
//     regs.F &= 0xF0;
//     // if (getFlag(CF)) regs.F |= 1<<4;
//     // if (getFlag(HF)) regs.F |= 1<<5;
//     // if (getFlag(NF)) regs.F |= 1<<6;
//     // if (getFlag(ZF)) regs.F |= 1<<7;
// }
#include <stdio.h>
#include <assert.h>
#include "Cpu.h"
#include "Typedefs.h"
#include "Logger.h"

struct Registers regs;

const int flag_c_mask = 1<<4;
const int flag_h_mask = 1<<5;
const int flag_n_mask = 1<<6;
const int flag_z_mask = 1<<7;

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
            Log("Error: invalid flag to get!", ERROR);
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
                Log("Error: invalid flag to set!", ERROR);
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
                Log("Error: invalid flag to set!", ERROR);
                exit(1);
        }
    }
}

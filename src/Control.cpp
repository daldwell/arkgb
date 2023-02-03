#include "Typedefs.h"
#include "Control.h"
#include "Interrupt.h"
#include <assert.h>

const int MASK_0 = 1;
const int MASK_1 = 1<<1;
const int MASK_2 = 1<<2;
const int MASK_3 = 1<<3;

byte JOYP = 0xFF;
byte Direction = 0xFF;
byte Action = 0xFF;

void KeyDown(JoyPadKey k) 
{
    switch(k)
    {
        case Up:
            Direction &= ~(MASK_2);
            break;
        case Down:
            Direction &= ~(MASK_3);
            break;
        case Left:
            Direction &= ~(MASK_1);
            break;
        case Right:
            Direction &= ~(MASK_0);
            break;
        case Start:
            Action &= ~(MASK_3);
            break;
        case Select:
            Action &= ~(MASK_2);
            break;
        case A:
            Action &= ~(MASK_0);
            break;
        case B:
            Action &= ~(MASK_1);
            break;
    }
}

void KeyUp(JoyPadKey k) 
{
    switch(k)
    {
        case Up:
            Direction |= MASK_2;
            break;
        case Down:
            Direction |= MASK_3;
            break;
        case Left:
            Direction |= MASK_1;
            break;
        case Right:
            Direction |= MASK_0;
            break;
        case Start:
            Action |= MASK_3;
            break;
        case Select:
            Action |= MASK_2;
            break;
        case A:
            Action |= MASK_0;
            break;
        case B:
            Action |= MASK_1;
            break;
    }
}

void ProcessKey(JoyPadKey k, bool pressed) 
{
    if (pressed) {
        KeyDown(k);
        IFRegister |= joypad_flag;
    } else {
        KeyUp(k);
    }

}